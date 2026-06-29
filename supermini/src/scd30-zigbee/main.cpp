#ifndef ZIGBEE_MODE_ED
#error "Zigbee end device mode is not selected. Add -D ZIGBEE_MODE_ED to build_flags."
#endif

#include "Zigbee.h"
#include "ep/ZigbeeTempSensor.h"
#include "ep/ZigbeeCarbonDioxideSensor.h"
#include "ep/ZigbeeAnalog.h"
#include <Adafruit_NeoPixel.h>
#include <esp_sleep.h>
#include <esp_system.h>
#include <Preferences.h>
#include "temp_reporter.h"
#include "voltage_reporter.h"

// Hardware
#define LED_PIN 8
#define SCL_PIN SCL1 // 7
#define SDA_PIN SDA1 // 6
#define ADC_PIN A0

// Zigbee endpoints. Temp+humidity+battery share one endpoint (ZigbeeTempSensor
// hosts the Relative Humidity and Power Config clusters too).
#define EP_ENV 1   // temperature + humidity + battery
#define EP_CO2 2   // carbon dioxide
#define EP_SLEEP 3 // sleep-duration control (analog output)

#define DEFAULT_SLEEP_S 1200 // 20 minutes

// Battery percentage is derived from the measured cell voltage (1S LiPo/Li-ion).
#define BATT_V_MIN 3.0f // 0%
#define BATT_V_MAX 4.2f // 100%

// Report battery % via the Power Config cluster. (The earlier rejoin failures
// were a weak-antenna RF problem, not this — safe to keep on with a good antenna.)
#define REPORT_BATTERY 1

// Pin to the coordinator's channel. All-channel steering is unreliable on the
// C6; locking to the ZHA network channel makes association succeed. Must match
// your ZHA network channel (HA -> ZHA -> network settings).
#define ZIGBEE_CHANNEL 15

// After a fresh join, stay awake this long so the coordinator can finish the
// interview AND the "Configuring" (binding + reporting setup) phase — both need
// the device responsive. Only applies to a steering join, not normal wakeups.
#define JOIN_SETTLE_MS 60000

// Pairing: tap the RESET button TAPS_TO_PAIR times, each within TAP_WINDOW_MS
// of the previous, to wipe the Zigbee NVRAM and join a fresh network.
// (BOOT/GPIO9 can't wake the C6 from deep sleep, so RESET is the trigger.)
#define TAPS_TO_PAIR 3
#define TAP_WINDOW_MS 3500 // forgiving window between RESET taps

// Persists across deep sleep cycles
RTC_DATA_ATTR static uint32_t sleepDurationS = DEFAULT_SLEEP_S;

ZigbeeTempSensor zbEnv(EP_ENV); // temperature + humidity + battery
ZigbeeCarbonDioxideSensor zbCO2(EP_CO2);
ZigbeeAnalog zbSleep(EP_SLEEP);

static void onSleepOutputChange(float value)
{
  // Called from Zigbee task — do NOT call any zbXxx functions here.
  if (value > 0)
  {
    sleepDurationS = (uint32_t)value;
    Serial.printf("Sleep duration set to %u s\n", sleepDurationS);
  }
}

static TemperatureReporter tempReporter;
static VoltageReporter voltageReporter(ADC_PIN, -1);

void setup()
{
  Adafruit_NeoPixel led(1, LED_PIN, NEO_GRB + NEO_KHZ800);
  led.begin();
  led.setBrightness(10);
  led.setPixelColor(0, led.Color(0, 0, 255));
  led.show();
  delay(100);
  led.setPixelColor(0, led.Color(0, 0, 0));
  led.show();

  Serial.begin(115200);

  // --- Triple-tap RESET to enter pairing mode ---
  // The RESET (EN) button resets the whole chip incl. the RTC domain, so the
  // tap counter must live in NVS (flash), not RTC memory. A timer wake is a
  // normal scheduled cycle (clears the counter). Our own ESP.restart() reboots
  // report ESP_RST_SW and are ignored, so a begin() reboot loop never counts as
  // taps. Anything else (RESET button or power-on) is a tap: count it, and if
  // it's not the Nth, persist it then hold a short window — if the user resets
  // again within it, this boot's clear never runs and the count carries over;
  // otherwise the window expires and we boot normally.
  bool pairingRequested = false;
  Preferences prefs;
  prefs.begin("pairing", false);
  if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER)
  {
    if (prefs.getUInt("taps", 0) != 0)
      prefs.putUInt("taps", 0);
  }
  else if (esp_reset_reason() != ESP_RST_SW)
  {
    uint32_t taps = prefs.getUInt("taps", 0) + 1;
    if (taps >= TAPS_TO_PAIR)
    {
      prefs.putUInt("taps", 0);
      pairingRequested = true;
      Serial.println("Triple-tap detected — entering pairing mode");
    }
    else
    {
      prefs.putUInt("taps", taps); // persist BEFORE the window so the next reset sees it
      Serial.printf("Reset tap %u/%u — tap RESET again to pair\n", taps, TAPS_TO_PAIR);
      for (uint32_t i = 0; i < taps; i++)
      {
        led.setPixelColor(0, led.Color(255, 255, 0)); // yellow = tap count
        led.show();
        delay(150);
        led.setPixelColor(0, 0);
        led.show();
        delay(150);
      }
      delay(TAP_WINDOW_MS);
      prefs.putUInt("taps", 0); // window expired without another tap
    }
  }
  prefs.end();

  if (pairingRequested)
  {
    led.setPixelColor(0, led.Color(255, 0, 255)); // magenta = pairing
    led.show();
  }

  // Init I2C and SCD30 before Zigbee to avoid interference
  Wire.begin(SDA_PIN, SCL_PIN);
  if (tempReporter.initSensor() != 0)
  {
    Serial.println("SCD30 init failed");
  }

  // Temperature + humidity + battery on one endpoint → native ZHA sensors.
  zbEnv.setManufacturerAndModel("ESP32C6", "SCD30-Zigbee");
  zbEnv.addHumiditySensor(0, 100, 0.1);               // min%, max%, tolerance%
#if REPORT_BATTERY
  zbEnv.setPowerSource(ZB_POWER_SOURCE_BATTERY, 100); // adds Power Config (battery %) cluster
#endif
  Zigbee.addEndpoint(&zbEnv);

  zbCO2.setManufacturerAndModel("ESP32C6", "SCD30-Zigbee");
  Zigbee.addEndpoint(&zbCO2);

  // Sleep duration: analog output → a Number control in HA.
  zbSleep.setManufacturerAndModel("ESP32C6", "SCD30-Zigbee");
  zbSleep.addAnalogOutput();
  zbSleep.onAnalogOutputChange(onSleepOutputChange);
  Zigbee.addEndpoint(&zbSleep);

  // Reliable deep-sleep rejoin (maintainer-recommended for sleepy EDs):
  //  - ed_timeout 2048 min: the coordinator keeps us as a child far longer than
  //    any sleep cycle. The default 64 min lets the parent forget us, after which
  //    the rejoin on wake is rejected ("Commissioning failed") — our exact bug.
  //  - keep_alive 60s: matches the longer-lived sleepy profile.
  // NOTE: ed_timeout is negotiated at JOIN time, so this only takes effect after a
  // fresh (re)pair, not a plain reboot.
  esp_zb_cfg_t zigbeeConfig = ZIGBEE_DEFAULT_ED_CONFIG();
  zigbeeConfig.nwk_cfg.zed_cfg.ed_timeout = ESP_ZB_ED_AGING_TIMEOUT_2048MIN;
  zigbeeConfig.nwk_cfg.zed_cfg.keep_alive = 60000;
  Zigbee.setTimeout(10000);

  // rx_on_when_idle stays at its default (true), matching the official sleepy example.

  // Single-channel scan = fast rejoin on wake (~77 ms vs ~261 ms all-channel).
  Zigbee.setScanDuration(2);
  Zigbee.setPrimaryChannelMask(1UL << ZIGBEE_CHANNEL);

  // pairingRequested -> erase_nvs = true: wipe stored network, become factory-new
  // and steer/join fresh. This also recovers a device stuck rejoining a dead network.
  Serial.println(pairingRequested ? "Starting Zigbee (pairing — erasing NVRAM)..."
                                  : "Starting Zigbee...");
  if (!Zigbee.begin(&zigbeeConfig, pairingRequested))
  {
    Serial.println("Zigbee failed — rebooting");
    delay(1000);
    ESP.restart();
  }

  // Max 802.15.4 TX power (+20 dBm) — the supermini's weak antenna gives a poor
  // link margin; this strengthens the fresh-join association and all reports.
  esp_zb_set_tx_power(20);
  int8_t txp = 0;
  esp_zb_get_tx_power(&txp);
  Serial.printf("TX power set to %d dBm\n", txp);

  Serial.println("Waiting to join network...");
  // Blink magenta whenever we're actually steering/joining (not yet connected),
  // regardless of whether this boot triggered the erase. A device that already
  // joined reconnects instantly and never enters this loop.
  bool blink = false;
  bool waited = false;
  while (!Zigbee.connected())
  {
    waited = true;
    delay(100);
    Serial.print(".");
    blink = !blink;
    led.setPixelColor(0, blink ? led.Color(255, 0, 255) : 0); // magenta = joining
    led.show();
  }
  Serial.println("\nJoined!");

  if (waited)
  {
    // Green flash to confirm a join that required steering (first join / pairing).
    led.setPixelColor(0, led.Color(0, 255, 0));
    led.show();
    delay(500);
    led.setPixelColor(0, 0);
    led.show();

    // Hold off deep sleep so the coordinator can complete the interview and the
    // "Configuring" (binding + reporting) phase — both require us to stay awake.
    // The Zigbee task services these requests in the background while we wait.
    Serial.printf("Fresh join — staying awake %us for interview/configuration\n",
                  JOIN_SETTLE_MS / 1000);
    uint32_t start = millis();
    bool blink = false;
    while (millis() - start < JOIN_SETTLE_MS)
    {
      delay(250);
      blink = !blink;
      led.setPixelColor(0, blink ? led.Color(0, 40, 0) : 0); // dim green pulse = settling
      led.show();
    }
    led.setPixelColor(0, 0);
    led.show();
  }

  // Give the coordinator time to establish connection before we report and sleep
  delay(1000);
}

void loop()
{
  SCD30Reading reading;
  if (tempReporter.measure(reading) == 0)
  {
    Serial.printf("T=%.2f°C  H=%.2f%%  CO2=%.0fppm\n",
                  reading.temperature, reading.humidity, reading.co2);
    zbEnv.setTemperature(reading.temperature); // °C
    zbEnv.setHumidity(reading.humidity);       // %
    zbEnv.report();                            // temperature + humidity
    zbCO2.setCarbonDioxide(reading.co2);       // ppm
    zbCO2.report();
  }
  else
  {
    Serial.println("SCD30 read failed");
  }

#if REPORT_BATTERY
  float voltage = voltageReporter.readVoltage();
  int battPct = (int)roundf((voltage - BATT_V_MIN) / (BATT_V_MAX - BATT_V_MIN) * 100.0f);
  battPct = constrain(battPct, 0, 100);
  Serial.printf("Battery: %.2fV -> %d%%\n", voltage, battPct);
  // Update the attribute only. Do NOT call reportBatteryPercentage(): the
  // bound-mode Power Config report asserts/crashes the stack on this SDK
  // (esp_zigbee_zcl_command.c). ZHA pulls battery via its configured reporting /
  // polling — this matches the official sleepy example, which never manual-reports it.
  zbEnv.setBatteryPercentage(battPct);
#endif

  // Allow time for all reports to be transmitted before sleeping
  delay(500);

  Serial.printf("Sleeping %us\n", sleepDurationS);
  esp_sleep_enable_timer_wakeup((uint64_t)sleepDurationS * 1000000ULL);
  esp_deep_sleep_start();
}
