#ifndef ZIGBEE_MODE_ED
#error "Zigbee end device mode is not selected. Add -D ZIGBEE_MODE_ED to build_flags."
#endif

#include "Zigbee.h"
#include "ep/ZigbeeAnalog.h"
#include <Adafruit_NeoPixel.h>
#include "temp_reporter.h"
#include "voltage_reporter.h"

// Hardware
#define LED_PIN 8
#define SCL_PIN SCL1  // 7
#define SDA_PIN SDA1  // 6
#define ADC_PIN A0

// Zigbee endpoints
#define EP_TEMP     1
#define EP_HUMIDITY 2
#define EP_CO2      3
#define EP_VOLTAGE  4
#define EP_SLEEP    5

#define DEFAULT_SLEEP_S 1200  // 20 minutes

// Persists across deep sleep cycles
RTC_DATA_ATTR static uint32_t sleepDurationS = DEFAULT_SLEEP_S;

ZigbeeAnalog zbTemp(EP_TEMP);
ZigbeeAnalog zbHumidity(EP_HUMIDITY);
ZigbeeAnalog zbCO2(EP_CO2);
ZigbeeAnalog zbVoltage(EP_VOLTAGE);
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

  // Init I2C and SCD30 before Zigbee to avoid interference
  Wire.begin(SDA_PIN, SCL_PIN);
  if (tempReporter.initSensor() != 0)
  {
    Serial.println("SCD30 init failed");
  }

  zbTemp.setManufacturerAndModel("ESP32C6", "SCD30-Zigbee");
  zbTemp.addAnalogInput();

  zbHumidity.setManufacturerAndModel("ESP32C6", "SCD30-Zigbee");
  zbHumidity.addAnalogInput();

  zbCO2.setManufacturerAndModel("ESP32C6", "SCD30-Zigbee");
  zbCO2.addAnalogInput();

  zbVoltage.setManufacturerAndModel("ESP32C6", "SCD30-Zigbee");
  zbVoltage.addAnalogInput();

  zbSleep.setManufacturerAndModel("ESP32C6", "SCD30-Zigbee");
  zbSleep.addAnalogInput();
  zbSleep.addAnalogOutput();
  zbSleep.onAnalogOutputChange(onSleepOutputChange);

  Zigbee.addEndpoint(&zbTemp);
  Zigbee.addEndpoint(&zbHumidity);
  Zigbee.addEndpoint(&zbCO2);
  Zigbee.addEndpoint(&zbVoltage);
  Zigbee.addEndpoint(&zbSleep);

  // keep_alive = 10s: important for sleepy devices so the coordinator doesn't drop them
  esp_zb_cfg_t zigbeeConfig = ZIGBEE_DEFAULT_ED_CONFIG();
  zigbeeConfig.nwk_cfg.zed_cfg.keep_alive = 10000;
  Zigbee.setTimeout(10000);

  Serial.println("Starting Zigbee...");
  if (!Zigbee.begin(&zigbeeConfig, false))
  {
    Serial.println("Zigbee failed — rebooting");
    delay(1000);
    ESP.restart();
  }

  Serial.println("Waiting to join network...");
  while (!Zigbee.connected())
  {
    delay(100);
    Serial.print(".");
  }
  Serial.println("\nJoined!");

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
    zbTemp.setAnalogInput(reading.temperature);
    zbTemp.reportAnalogInput();
    zbHumidity.setAnalogInput(reading.humidity);
    zbHumidity.reportAnalogInput();
    zbCO2.setAnalogInput(reading.co2);
    zbCO2.reportAnalogInput();
  }
  else
  {
    Serial.println("SCD30 read failed");
  }

  float voltage = voltageReporter.readVoltage();
  zbVoltage.setAnalogInput(voltage);
  zbVoltage.reportAnalogInput();

  zbSleep.setAnalogInput((float)sleepDurationS);
  zbSleep.reportAnalogInput();

  // Allow time for all reports to be transmitted before sleeping
  delay(500);

  Serial.printf("Sleeping %us\n", sleepDurationS);
  esp_sleep_enable_timer_wakeup((uint64_t)sleepDurationS * 1000000ULL);
  esp_deep_sleep_start();
}
