#include <PubSubClient.h>
#include "reporter.h"

class NTPReporter : Reporter
{
public:
  NTPReporter(PubSubClient &mqttClient,
              const char *topic,
              int gmtOffset,
              int daylightOffsetSec,
              const char *ntpServer) : Reporter(mqttClient, topic),
                                       gmtOffset(gmtOffset),
                                       daylightOffsetSec(daylightOffsetSec),
                                       ntpServer(ntpServer) {}

  int report() override
  {
    int result = connect();

    if (result != 0)
    {
      return result;
    }

    // Get current time
    time_t now = time(nullptr);
    struct tm *timeinfo = localtime(&now);
    char timeStr[64];
    strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", timeinfo);

    // Publish
    String timeTopic = String(mqttTopic) + "/lastSeen";
    if (mqtt.publish(timeTopic.c_str(), timeStr, true))
    {
      Serial.printf("Published time to MQTT: %s --> %s\n", timeStr, timeTopic.c_str());
    }
    else
    {
      Serial.println("Failed to publish time to MQTT");

      return 1;
    }

    mqtt.loop();
    delay(50);

    return 0;
  }

  int connect()
  {
    // Configure NTP
    configTime(gmtOffset, daylightOffsetSec, ntpServer);
    Serial.println("Waiting for NTP time sync...");
    now = time(nullptr);

    int retries = 0;
    while (now < 8 * 3600 * 2 && retries < 20)
    {
      delay(500);
      Serial.print(".");
      now = time(nullptr);
      retries++;
    }

    return now < 8 * 3600 * 2 ? 1 : 0;
  }

private:
  int gmtOffset;         // seconds
  int daylightOffsetSec; // seconds
  const char *ntpServer; // e.g. "pool.ntp.org"
  time_t now;
};