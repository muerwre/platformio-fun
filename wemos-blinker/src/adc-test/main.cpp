#include <Arduino.h>
#include <Wire.h>

// ADC_MODE(ADC_VCC);

void setup()
{
  analogReference(INTERNAL); // Set ADC reference to internal 1.1V
  Wire.begin(0, 2);
  Serial.begin(74880);
  pinMode(PIN_A0, INPUT);
}

void loop()
{
  int value = analogRead(A0);
  delay(1000);
  Serial.printf("ADC: %d\n", value);
  Serial.printf("VCC: %d\n", ESP.getVcc());
  delay(1000);
}