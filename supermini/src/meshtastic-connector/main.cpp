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

// Absolute time (seconds since start) the next scheduled telemetry is due. We
// always sleep until THIS deadline, never a fresh full interval, so PIR motion
// wakes can't keep postponing telemetry. Advanced each time telemetry is sent.
RTC_DATA_ATTR int64_t nextTelemetrySec = 0;

static const int64_t INTERVAL_SEC = (int64_t)MESH_SEND_INTERVAL_MIN * 60;
static const int64_t MIN_RETRY_SEC = 60; // if a due send fails, retry soon, not in a full interval

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

// Seconds to sleep so we wake at the telemetry deadline (or sooner on motion),
// clamped to a sane range. Used by every sleep path so nothing resets the clock.
static uint32_t secondsUntilTelemetry(int64_t now)
{
  int64_t remaining = nextTelemetrySec - now;
  if (remaining < MIN_RETRY_SEC)
    remaining = MIN_RETRY_SEC;
  if (remaining > INTERVAL_SEC)
    remaining = INTERVAL_SEC;
  return (uint32_t)remaining;
}

void setup()
{
  Serial.begin(115200);
  delay(200);

  // Seconds since start (monotonic) doubles as the nonce; also report the gap
  // since the previous wake (≈ how long we were asleep).
  int64_t now = secondsSinceStart();
  if (nextTelemetrySec == 0) // first boot: schedule the first telemetry
    nextTelemetrySec = now + INTERVAL_SEC;

  Serial.printf("Woke at %llds (%llds since last wake), telemetry due at %llds\n",
                (long long)now, (long long)(now - lastWakeSec), (long long)nextTelemetrySec);
  lastWakeSec = now;

  bool motion = (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT1);
  bool telemetryDue = now >= nextTelemetrySec;

  // Debounce intruder alerts: ignore motion within 5 min of the last alert. But
  // if telemetry is also due now, fall through and connect for it (we just skip
  // the intruder text). We never touch nextTelemetrySec here, so a motion storm
  // can't push the schedule back.
  bool suppressedIntruder =
      motion && intruderSent && (now - lastIntruderSec) < INTRUDER_COOLDOWN_MIN * 60;
  if (suppressedIntruder && !telemetryDue)
  {
    Serial.printf("Intruder ignored (%llds since last alert), %llds until telemetry\n",
                  (long long)(now - lastIntruderSec), (long long)(nextTelemetrySec - now));
    mesh.sleepNow(secondsUntilTelemetry(now));
    return;
  }

  // Read sensors before BLE starts — I2C and NimBLE conflict on ESP32-C6 if the
  // bus is still active when the radio initialises. We read on every wake so a
  // motion wake can piggyback telemetry on the same session.
  bool sensorOk = sensors.begin();
  SensorReading reading;
  if (sensorOk)
    reading = sensors.read();
  Wire.end(); // release I2C before BLE init

  if (mesh.connect())
  {
    if (motion && !suppressedIntruder)
    {
      // PIR wakeup -> intruder text alert.
      lastIntruderSec = now;
      intruderSent = true;
      char message[48];
      snprintf(message, sizeof(message), "Intruder! (nonce: %lu)", (unsigned long)(uint32_t)now);
      mesh.send(message);
    }

    // Telemetry follows in the same session. Sending fresh telemetry — whether
    // scheduled or piggybacked on motion — resets the deadline, so we never
    // double-send within an interval.
    if (sensorOk)
    {
      mesh.sendTelemetry(reading.temperature, reading.humidity, reading.pressure, reading.voltage);
      nextTelemetrySec = now + INTERVAL_SEC;
      Serial.printf("Sent: temp=%.2f°C hum=%.2f%% pres=%.2fhPa volt=%.3fV; next telemetry at %llds\n",
                    reading.temperature, reading.humidity, reading.pressure, reading.voltage,
                    (long long)nextTelemetrySec);
    }
    else if (!motion)
    {
      // Nothing else to report on a plain timer wake — flag the bad read.
      mesh.send("sensor error");
    }
  }

  // Sleep until the telemetry deadline (or sooner if motion fires the PIR).
  mesh.sleepForInterval(secondsUntilTelemetry(now));
}

void loop() {}
