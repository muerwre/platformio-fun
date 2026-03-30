#include <PubSubClient.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_SCD30.h>
#include "reporter.h"

#define RETRIES_COUNT 5
#define MEASURE_INTERVAL 10 // seconds
// we need some time to spin up sensor, that is a max wait time, could take less
#define WARMUP_INTERVAL MEASURE_INTERVAL * 1.25

class TemperatureReporter : Reporter
{
public:
  TemperatureReporter(PubSubClient &mqttClient,
                      uint8_t sclPin, uint8_t sdaPin,
                      const char *topic) : Reporter(mqttClient, topic), sclPin(sclPin), sdaPin(sdaPin) {}

  int initSensor()
  {
    Serial.println("Adafruit SCD30 test!");
    int retries = RETRIES_COUNT;
    Wire.begin(sdaPin, sclPin);

    // default speed (100kHz) seems to cause issues with the SCD30, so set to 50kHz
    // https://forums.adafruit.com/viewtopic.php?t=217459
    Wire.setClock(10000L); // 10kHz

    while (!scd30.begin() && retries > 0)
    {
      Serial.printf("Failed to find SCD30 chip, retrying... (%d retries left)\n", retries);
      Wire.begin(sdaPin, sclPin);
      delay(1000);
      mqtt.loop();
      retries--;
    }

    if (retries <= 0)
    {
      Serial.println("Failed to find SCD30 chip after retries, giving up");
      return 1;
    }

    scd30.setMeasurementInterval(2);

    sensor_available = true;
    Serial.printf("Sensor found! Measurement Interval: %d seconds\n", scd30.getMeasurementInterval());

    return 0;
  }

  int report() override
  {
    if (measure() != 0)
    {
      return 1;
    }

    String tempTopic = String(mqttTopic) + "/temperature";
    String humiTopic = String(mqttTopic) + "/humidity";
    String co2Topic = String(mqttTopic) + "/co2";

    Serial.printf("Temperature: %.2f°C, Humidity: %.2f%%, CO2: %.2f ppm\n",
                  scd30.temperature, scd30.relative_humidity, scd30.CO2);

    if (!mqtt.publish(tempTopic.c_str(), String(scd30.temperature, 2).c_str(), true))
    {
      Serial.println("Failed to publish temperature to MQTT");
      return 1;
    }
    Serial.printf("Published temperature to MQTT: %.2f°C --> %s\n", scd30.temperature, tempTopic.c_str());

    mqtt.loop();

    if (!mqtt.publish(humiTopic.c_str(), String(scd30.relative_humidity, 2).c_str(), true))
    {
      Serial.println("Failed to publish humidity to MQTT");
      return 1;
    }
    Serial.printf("Published humidity to MQTT: %.2f%% --> %s\n", scd30.relative_humidity, humiTopic.c_str());

    mqtt.loop();

    if (scd30.CO2 > double(0) && !mqtt.publish(co2Topic.c_str(), String(scd30.CO2, 2).c_str(), true))
    {
      Serial.println("Failed to publish CO2 to MQTT");
      return 1;
    }
    Serial.printf("Published CO2 to MQTT: %.2f ppm --> %s\n", scd30.CO2, co2Topic.c_str());

    mqtt.loop();

    // doing this at the end of report gives us a change to catch next measurement
    // earlier than MEASURE_INTERVAL, because setting it at the beginning of report would cause the
    // first measurement to be ready after MEASURE_INTERVAL
    if (!scd30.setMeasurementInterval(MEASURE_INTERVAL))
    {
      Serial.println("Failed to set measurement interval");
    }

    scd30.startContinuousMeasurement();
    Serial.printf("New measurement Interval: %d seconds\n", scd30.getMeasurementInterval());

    return 0;
  }

private:
  uint8_t sclPin;
  uint8_t sdaPin;
  Adafruit_SCD30 scd30;
  bool sensor_available = false;

  int measure()
  {
    if (!sensor_available)
    {
      Serial.println("Sensor not available, skipping report");
      return 1;
    }

    // sensor warmup can take up to MEASURE_INTERVAL seconds, we wait here

    Serial.print("Waiting for data from sensor");
    unsigned long now = millis();
    while (!scd30.dataReady() && millis() - now < WARMUP_INTERVAL * 1e3)
    {
      Serial.print(".");
      delay(300);
      mqtt.loop();
    }
    Serial.printf(" (took %d seconds)\n", int((millis() - now) / 1000));

    if (!scd30.dataReady())
    {
      Serial.println("Data not ready, skipping report");
      return 1;
    }

    // first reports are often invalid, wait until good ones

    now = millis();
    Serial.print("Reading data from sensor");
    while (!scd30.read() || scd30.CO2 == 0 || !isfinite(scd30.CO2))
    {
      Serial.print(".");
      delay(1000);
      mqtt.loop();

      if (millis() - now > WARMUP_INTERVAL * 1e3)
      {
        Serial.println("Failed to read valid data from sensor after retries, skipping report");
        return 1;
      }
    }

    Serial.printf(" (took %d seconds)\n", int((millis() - now) / 1000));

    return 0;
  }
};