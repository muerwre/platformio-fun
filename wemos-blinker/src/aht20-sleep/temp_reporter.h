#include <PubSubClient.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h>
#include <Adafruit_AHTX0.h>
#include "reporter.h"

#define SEALEVELPRESSURE_HPA (1013.25)

class TemperatureReporter : Reporter
{
public:
  TemperatureReporter(PubSubClient &mqttClient,
                      uint8_t sclPin, uint8_t sdaPin,
                      const char *topic) : Reporter(mqttClient, topic), sclPin(sclPin), sdaPin(sdaPin) {}

  void initSensor()
  {
    if (!bmp.begin())
    {
      Serial.println("Could not find a valid BMP280 sensor, check wiring!");
    }
    else
    {
      Serial.println("BMP280 Found!");
      bmp_temp = bmp.getTemperatureSensor();
      bmp_temp->printSensorDetails();
      bmp_press = bmp.getPressureSensor();
      bmp_press->printSensorDetails();
      bmpInitialized = true;
    }

    if (!aht.begin())
    {
      Serial.println("Failed to find AHT10/AHT20 chip");
    }
    else
    {
      ahtInitialized = true;

      Serial.println("AHT10/AHT20 Found!");
      aht_temp = aht.getTemperatureSensor();
      aht_temp->printSensorDetails();

      aht_humidity = aht.getHumiditySensor();
      aht_humidity->printSensorDetails();
    }
  }

  int report() override
  {
    String tempTopic = String(mqttTopic) + "/temperature";
    String tempBMPTopic = String(mqttTopic) + "/temperatureBMP";
    String humiTopic = String(mqttTopic) + "/humidity";
    String pressureBMPTopic = String(mqttTopic) + "/pressureBMP";

    // Report AHT sensor data if initialized
    if (ahtInitialized)
    {
      sensors_event_t humidity;
      sensors_event_t temp;

      aht_temp->getEvent(&temp);
      aht_humidity->getEvent(&humidity);

      Serial.printf("Humidity: %.2f%%, Temperature: %.2f°C\n", humidity.relative_humidity, temp.temperature);

      if (mqtt.publish(tempTopic.c_str(), String(temp.temperature, 2).c_str(), true))
      {
        Serial.printf("Published temperature to MQTT: %.2f°C --> %s\n", temp.temperature, tempTopic.c_str());
      }
      else
      {
        Serial.println("Failed to publish temperature to MQTT");
        return 1;
      }

      mqtt.loop();
      delay(50);

      if (mqtt.publish(humiTopic.c_str(), String(humidity.relative_humidity, 2).c_str(), true))
      {
        Serial.printf("Published humidity to MQTT: %.2f%% --> %s\n", humidity.relative_humidity, humiTopic.c_str());
      }
      else
      {
        Serial.println("Failed to publish humidity to MQTT");
        return 1;
      }

      mqtt.loop();
      delay(50);
    }
    else
    {
      Serial.println("AHT sensor not initialized, skipping AHT readings");
    }

    // Report BMP sensor data if initialized
    if (bmpInitialized)
    {
      // Read and publish BMP280 temperature
      float bmpTemp = bmp.readTemperature();
      if (mqtt.publish(tempBMPTopic.c_str(), String(bmpTemp, 2).c_str(), true))
      {
        Serial.printf("Published BMP temperature to MQTT: %.2f°C --> %s\n", bmpTemp, tempBMPTopic.c_str());
      }
      else
      {
        Serial.println("Failed to publish BMP temperature to MQTT");
        return 1;
      }

      mqtt.loop();
      delay(50);

      // Read and publish BMP280 pressure
      float bmpPressure = bmp.readPressure() * 0.00750062; // Convert Pa to mmHg
      if (mqtt.publish(pressureBMPTopic.c_str(), String(bmpPressure, 2).c_str(), true))
      {
        Serial.printf("Published BMP pressure to MQTT: %.2f mmHg --> %s\n", bmpPressure, pressureBMPTopic.c_str());
      }
      else
      {
        Serial.println("Failed to publish BMP pressure to MQTT");
        return 1;
      }

      mqtt.loop();
      delay(50);
    }
    else
    {
      Serial.println("BMP sensor not initialized, skipping BMP readings");
    }

    return 0;
  }

private:
  uint8_t sclPin;
  uint8_t sdaPin;
  Adafruit_Sensor *aht_humidity, *aht_temp, *bmp_temp, *bmp_press;
  Adafruit_BMP280 bmp;
  Adafruit_AHTX0 aht;
  bool bmpInitialized = false;
  bool ahtInitialized = false;
};