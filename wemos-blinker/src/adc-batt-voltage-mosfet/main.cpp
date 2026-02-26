#include <Arduino.h>
#include <ESP8266WiFi.h>

void setup()
{
  Serial.begin(115200);
  pinMode(D1, OUTPUT);
}

void loop()
{
  digitalWrite(D1, LOW);
  Serial.printf("Low: %d\n", analogRead(A0));
  delay(1000);
  digitalWrite(D1, HIGH);
  delay(50);
  Serial.printf("High %d\n", analogRead(A0));
  delay(1000);
}