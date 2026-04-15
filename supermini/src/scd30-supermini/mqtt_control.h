#pragma once
#include <PubSubClient.h>

class MqttControl
{
public:
  MqttControl(PubSubClient &mqttClient, const char *topic, long defaultSleepSeconds)
      : mqtt(mqttClient), mqttTopic(topic), sleepDurationSeconds(defaultSleepSeconds),
        defaultSleepSeconds(defaultSleepSeconds) {}

  void callback(char *topic, byte *payload, unsigned int length)
  {
    String intervalTopic = String(mqttTopic) + "/update_interval_mins";
    if (String(topic) != intervalTopic)
    {
      return;
    }

    char value[16];
    unsigned int len = min(length, (unsigned int)sizeof(value) - 1);
    memcpy(value, payload, len);
    value[len] = '\0';
    long mins = atol(value);
    if (mins > 0)
    {
      sleepDurationSeconds = mins * 60;
      Serial.printf("Update interval from MQTT: %ld minutes (%ld seconds)\n", mins, sleepDurationSeconds);
    }
    intervalReceived = true;
  }

  void readUpdateInterval()
  {
    String intervalTopic = String(mqttTopic) + "/update_interval_mins";
    mqtt.subscribe(intervalTopic.c_str());

    unsigned long start = millis();
    while (!intervalReceived && millis() - start < 2000)
    {
      mqtt.loop();
      delay(50);
    }

    mqtt.unsubscribe(intervalTopic.c_str());

    if (!intervalReceived || sleepDurationSeconds <= 0)
    {
      sleepDurationSeconds = defaultSleepSeconds;
      Serial.printf("No valid interval from MQTT, using default: %ld seconds\n", defaultSleepSeconds);
      mqtt.publish(intervalTopic.c_str(), String(defaultSleepSeconds / 60).c_str(), true);
      mqtt.loop();
      delay(100);
    }
  }

  long getSleepDurationSeconds() { return sleepDurationSeconds; }

private:
  PubSubClient &mqtt;
  const char *mqttTopic;
  long sleepDurationSeconds;
  long defaultSleepSeconds;
  volatile bool intervalReceived = false;
};
