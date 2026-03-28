#include <PubSubClient.h>
#pragma once

class Reporter
{
public:
  Reporter(PubSubClient &mqttClient, const char *topic) : mqtt(mqttClient), mqttTopic(topic) {}
  virtual ~Reporter() = default;

  virtual int report() = 0; // Pure virtual function

protected:
  PubSubClient &mqtt;
  const char *mqttTopic;
};