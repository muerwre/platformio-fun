#include "reporter.h"

class VoltageReporter : Reporter
{
public:
  VoltageReporter(PubSubClient &mqttClient,
                  int adcPin,
                  /** Set to -1 if you don't have MOSFET */
                  int mosfetControlPin,
                  const char *topic) : Reporter(mqttClient, topic),
                                       adcPin(adcPin),
                                       mosfetControlPin(mosfetControlPin)
  {
  }

  int report() override
  {
    int adcValue = measure();

    float voltage = (adcValue / ADC_MAX_VALUE) * VOLTAGE_DIVIDER_RATIO;
    float batteryPercentage = min(
        100.0,
        max(0.0,
            (adcValue - ADC_MIN_VALUE) / (ADC_MAX_VALUE - ADC_MIN_VALUE) * 100.0));

    if (adcValue < ADC_MIN_REPORTING_VALUE)
    {
      Serial.printf("ADC is below minimum reporting value, skipping voltage report: %d\n", adcValue);
      return 1;
    }

    if (adcValue > ADC_MAX_VALUE)
    {
      Serial.printf("ADC value is above maximum, something's wrong: %d\n", adcValue);
      return 1;
    }

    String voltTopic = String(mqttTopic) + "/voltage";
    if (mqtt.publish(voltTopic.c_str(), String(voltage, 2).c_str(), true))
    {
      Serial.printf("Published voltage to MQTT: %.2f V --> %s\n", voltage, voltTopic.c_str());
    }
    mqtt.loop();
    delay(50);

    String adcTopic = String(mqttTopic) + "/adc";
    if (mqtt.publish(adcTopic.c_str(), String(adcValue).c_str(), true))
    {
      Serial.printf("Published ADC value to MQTT: %d --> %s\n", adcValue, adcTopic.c_str());
    }

    mqtt.loop();
    delay(50);

    String percentTopic = String(mqttTopic) + "/percentage";
    if (mqtt.publish(percentTopic.c_str(), String(batteryPercentage, 2).c_str(), true))
    {
      Serial.printf("Published battery percentage to MQTT: %.2f%% --> %s\n", batteryPercentage, percentTopic.c_str());
    }

    mqtt.loop();
    delay(50);

    return 0;
  }

private:
  int adcPin;
  int mosfetControlPin;

  static constexpr float ADC_MAX_VALUE = 1023.0;       // Maximum ADC value (10-bit ADC)
  static constexpr float VOLTAGE_DIVIDER_RATIO = 3.55; // It's input volage, I suppose
  static constexpr float ADC_MIN_VALUE = 640;          // 2.7v
  static constexpr float ADC_MIN_REPORTING_VALUE = 50; // Below this ADC value, we won't report voltage

  int measure()
  {
    Serial.printf("Value before enabling MOSFET: %d\n", analogRead(adcPin));

    if (mosfetControlPin >= 0)
    {
      pinMode(mosfetControlPin, OUTPUT);
      digitalWrite(mosfetControlPin, HIGH); // Power on the voltage divider circuit
      delay(500);                           // Wait for the voltage to stabilize
    }

    int adcValue = analogRead(adcPin);

    if (mosfetControlPin >= 0)
    {
      digitalWrite(mosfetControlPin, LOW); // Power off the voltage divider circuit
    }

    return adcValue;
  }
};