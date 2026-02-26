#include <PubSubClient.h>
#include <DHT.h>
#include "reporter.h"

class TemperatureReporter : Reporter
{
public:
  TemperatureReporter(PubSubClient &mqttClient,
                      DHT &dhtSensor,
                      const char *topic) : Reporter(mqttClient, topic),
                                           dht(dhtSensor)
  {
  }

  int report() override
  {
    std::pair<float, float> measurement = measure();
    float tempC = measurement.first;
    float humi = measurement.second;

    if (isnan(humi) || isnan(tempC))
    {
      Serial.println("Failed to read from DHT sensor!");

      return 1;
    }

    Serial.printf("Humidity: %.2f%%, Temperature: %.2f°C\n", humi, tempC);
    String tempTopic = String(mqttTopic) + "/temperature";
    String humiTopic = String(mqttTopic) + "/humidity";

    if (mqtt.publish(tempTopic.c_str(), String(tempC, 2).c_str(), true))
    {
      Serial.printf("Published temperature to MQTT: %.2f°C --> %s\n", tempC, tempTopic.c_str());
    }
    else
    {
      Serial.println("Failed to publish temperature to MQTT");

      return 1;
    }

    mqtt.loop();
    delay(50);

    if (mqtt.publish(humiTopic.c_str(), String(humi, 2).c_str(), true))
    {
      Serial.printf("Published humidity to MQTT: %.2f%% --> %s\n", humi, humiTopic.c_str());
    }
    else
    {
      Serial.println("Failed to publish humidity to MQTT");

      return 1;
    }

    mqtt.loop();

    return 0;
  }

private:
  DHT &dht;

  std::pair<float, float> measure()
  {
    // read humidity
    float humi = dht.readHumidity();
    // read temperature as Celsius
    float tempC = dht.readTemperature();

    return std::make_pair(tempC, humi);
  }
};