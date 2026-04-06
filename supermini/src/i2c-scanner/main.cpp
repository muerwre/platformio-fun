#include <Arduino.h>
#include <Wire.h>

/**
 * It seeps that supermini lets us use almost any GP pin for SDA or SCL!
 */
#define SDA_PIN 20
#define SCL_PIN 19

void setup()
{
  Serial.begin(115200);
  Wire.begin(SDA_PIN, SCL_PIN);
  Serial.println("\nI2C Scanner");
}

void loop()
{
  byte error, address;
  int count = 0;

  Serial.println("Scanning...");

  for (address = 1; address < 127; address++)
  {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();

    if (error == 0)
    {
      Serial.printf("Device found at 0x%02X\n", address);
      count++;
    }
    else if (error == 4)
    {
      Serial.printf("Unknown error at 0x%02X\n", address);
    }
  }

  if (count == 0)
  {
    Serial.println("No devices found");
  }
  else
  {
    Serial.printf("%d device(s) found\n", count);
  }

  delay(3000);
}
