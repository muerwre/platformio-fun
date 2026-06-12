#include <Arduino.h>
#include <esp_sleep.h>
#include <sys/time.h>
#include "mesh_connector.h"
#include "sensors.h"

// --- Pin configuration ---
#define LED_PIN 8    // onboard WS2812
#define WAKEUP_PIN 5 // PIR motion sensor (GPIO0-7 wakeup range; GPIO5 is a strapping pin)
#define SDA_PIN 20   // AHT20 + BMP280 I2C
#define SCL_PIN 19
#define INTRUDER_COOLDOWN_MIN 5 // suppress repeat intruder alerts within this window

MeshConnector mesh(LED_PIN, WAKEUP_PIN);
Sensors sensors(SDA_PIN, SCL_PIN);

// Wall clock is preserved across deep sleep (the RTC keeps running). We zero it
// on first power-on, so it reads seconds since start.
RTC_DATA_ATTR bool timeStarted = false;
RTC_DATA_ATTR int64_t lastWakeSec = 0;
RTC_DATA_ATTR int64_t lastIntruderSec = 0;
RTC_DATA_ATTR bool intruderSent = false;

static int64_t secondsSinceStart()
{
  struct timeval tv = {0, 0};
  if (!timeStarted)
  {
    settimeofday(&tv, nullptr); // start the clock at zero on first boot
    timeStarted = true;
    return 0;
  }
  gettimeofday(&tv, nullptr);
  return tv.tv_sec;
}

void setup()
{
  Serial.begin(115200);
  delay(200);

  // Seconds since start (monotonic) doubles as the nonce; also report the gap
  // since the previous wake (≈ how long we were asleep).
  int64_t now = secondsSinceStart();
  Serial.printf("Woke at %llds (%llds since last wake)\n",
                (long long)now, (long long)(now - lastWakeSec));
  lastWakeSec = now;

  bool motion = (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT1);

  // Debounce intruder alerts: ignore motion within 5 min of the last alert and
  // go straight back to sleep without bothering to connect.
  if (motion && intruderSent && (now - lastIntruderSec) < INTRUDER_COOLDOWN_MIN * 60)
  {
    Serial.printf("Intruder ignored, only %llds since last alert\n",
                  (long long)(now - lastIntruderSec));
    mesh.sleepNow();
    return;
  }

  // Read sensors before BLE starts — I2C and NimBLE conflict on ESP32-C6
  // if the bus is still active when the radio initialises.
  bool sensorOk = false;
  SensorReading reading;
  if (!motion)
  {
    sensorOk = sensors.begin();
    if (sensorOk)
      reading = sensors.read();
    Wire.end(); // release I2C before BLE init
  }

  if (mesh.connect())
  {
    if (motion)
    {
      // PIR wakeup -> intruder text alert.
      lastIntruderSec = now;
      intruderSent = true;
      char message[48];
      snprintf(message, sizeof(message), "Intruder! (nonce: %lu)", (unsigned long)(uint32_t)now);
      mesh.send(message);
    }
    else if (sensorOk)
    {
      mesh.sendTelemetry(reading.temperature, reading.humidity, reading.pressure, reading.voltage);
      Serial.printf("Sent: temp=%.2f°C hum=%.2f%% pres=%.2fhPa volt=%.3fV\n",
                    reading.temperature, reading.humidity, reading.pressure, reading.voltage);
    }
    else
    {
      mesh.send("sensor error");
    }
  }

  mesh.sleepForInterval(); // deep sleep until the timer or PIR fires
}

void loop() {}
