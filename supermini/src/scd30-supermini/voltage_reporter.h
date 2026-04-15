#include "reporter.h"
#include <SPI.h>

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
    pinMode(adcPin, INPUT);
  }

  int report() override
  {
    int adcValue = measure();
    int mv = analogReadMilliVolts(adcPin);
    float voltage = (mv / 1000.0) * VOLTAGE_DIVIDER_RATIO;

    Serial.printf("ADC: %d\tmV: %d\tVoltage: %.2f V\n", adcValue, mv, voltage);

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

    return 0;
  }

private:
  int adcPin;
  int mosfetControlPin;

  static constexpr float VOLTAGE_DIVIDER_RATIO = 2.039; // (R1 + R2) / R2, calibrated

  int measure()
  {

    if (mosfetControlPin >= 0)
    {
      Serial.printf("Value before enabling MOSFET: %d\n", analogRead(adcPin));
      Serial.printf("Mosfet pin is %d\n", mosfetControlPin);
      pinMode(mosfetControlPin, INPUT);
      Serial.printf("Mosfet value before read %d\n", digitalRead(mosfetControlPin));
      delay(500);
      pinMode(mosfetControlPin, OUTPUT);
      digitalWrite(mosfetControlPin, HIGH); // Power on the voltage divider circuit
      delay(500);                           // Wait for the voltage to stabilize
    }

    int adcValue = analogRead(adcPin);

    if (mosfetControlPin >= 0)
    {
      digitalWrite(mosfetControlPin, LOW); // Power off the voltage divider circuit
      delay(500);
      pinMode(mosfetControlPin, INPUT);
      Serial.printf("Mosfet value after read %d\n", digitalRead(mosfetControlPin));
    }

    return adcValue;
  }
};