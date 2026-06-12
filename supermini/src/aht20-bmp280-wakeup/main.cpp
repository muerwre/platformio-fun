#include <Arduino.h>
#include <esp_sleep.h>
#include "status_led.h"
#include "sensors.h"

#define LED_PIN 8       // onboard WS2812
#define WAKEUP_PIN 4    // motion sensor IRQ (must be GPIO0-7 for C6 deep-sleep wakeup)
#define SDA_PIN 20      // AHT20 + BMP280 I2C
#define SCL_PIN 19
#define SLEEP_SECONDS 10 // timer wakeup interval

StatusLed led(LED_PIN);
Sensors sensors(SDA_PIN, SCL_PIN);

static void goToDeepSleep()
{
  led.off();
  // Wake on either the timer, or motion (WAKEUP_PIN driven HIGH).
  esp_sleep_enable_timer_wakeup((uint64_t)SLEEP_SECONDS * 1000000ULL);
  esp_sleep_enable_ext1_wakeup(1ULL << WAKEUP_PIN, ESP_EXT1_WAKEUP_ANY_HIGH);
  Serial.flush();
  esp_deep_sleep_start();
}

void setup()
{
  Serial.begin(115200);
  delay(200);
  led.begin();
  pinMode(WAKEUP_PIN, INPUT);

  // Motion wakeup: just show red and go back to sleep (no reading).
  if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT1)
  {
    Serial.println("Motion wakeup -> red");
    led.red();
    // Wait for motion to clear so we don't immediately re-wake (cap at 60s).
    uint32_t start = millis();
    while (digitalRead(WAKEUP_PIN) == HIGH && millis() - start < 60000)
      delay(50);
    goToDeepSleep();
    return;
  }

  // Timer wakeup (or first boot): read temp/humidity, blue LED while doing it.
  Serial.println("Timer wakeup -> reading sensor");
  led.blue();
  if (sensors.begin())
  {
    SensorReading r = sensors.read();
    Serial.printf("temp=%.2f C  humidity=%.1f %%  pressure=%.1f hPa\n",
                  r.temperature, r.humidity, r.pressure);
  }
  else
  {
    Serial.println("AHT20 init failed");
  }

  goToDeepSleep();
}

void loop() {}
