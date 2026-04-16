#pragma once
#include "reporter.h"

class VoltageReporter : public Reporter
{
public:
  VoltageReporter(int adcPin, int mosfetControlPin)
      : adcPin(adcPin), mosfetControlPin(mosfetControlPin)
  {
    pinMode(adcPin, INPUT);
  }

  // Returns voltage in volts
  float readVoltage()
  {
    int adcRaw = measure();
    int mv = analogReadMilliVolts(adcPin);
    float voltage = (mv / 1000.0f) * VOLTAGE_DIVIDER_RATIO;
    Serial.printf("ADC: %d\tmV: %d\tVoltage: %.2fV\n", adcRaw, mv, voltage);
    return voltage;
  }

  int report() override { return 0; }

private:
  int adcPin;
  int mosfetControlPin;

  static constexpr float VOLTAGE_DIVIDER_RATIO = 2.039f;  // (R1+R2)/R2, calibrated

  int measure()
  {
    if (mosfetControlPin >= 0)
    {
      pinMode(mosfetControlPin, OUTPUT);
      digitalWrite(mosfetControlPin, HIGH);
      delay(500);
    }

    int value = analogRead(adcPin);

    if (mosfetControlPin >= 0)
    {
      digitalWrite(mosfetControlPin, LOW);
      delay(500);
      pinMode(mosfetControlPin, INPUT);
    }

    return value;
  }
};
