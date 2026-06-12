#pragma once
#include <Wire.h>
#include <Adafruit_AHTX0.h>
#include <Adafruit_BMP280.h>

struct SensorReading
{
  float temperature; // °C  (AHT20)
  float humidity;    // %RH (AHT20)
  float pressure;    // hPa (BMP280, NAN if absent)
};

// AHT20 + BMP280 combo on I2C. AHT20 gives temperature + humidity, BMP280 adds
// barometric pressure. AHT20 is treated as required; BMP280 is optional.
class Sensors
{
public:
  Sensors(uint8_t sda, uint8_t scl) : sda(sda), scl(scl) {}

  bool begin()
  {
    Wire.begin(sda, scl);
    bool ahtOk = aht.begin();
    bmpOk = bmp.begin(0x76) || bmp.begin(0x77); // both common BMP280 addresses
    return ahtOk;
  }

  SensorReading read()
  {
    SensorReading r{NAN, NAN, NAN};

    sensors_event_t humidity, temp;
    if (aht.getEvent(&humidity, &temp))
    {
      r.temperature = temp.temperature;
      r.humidity = humidity.relative_humidity;
    }
    if (bmpOk)
      r.pressure = bmp.readPressure() / 100.0f; // Pa -> hPa

    return r;
  }

private:
  uint8_t sda, scl;
  Adafruit_AHTX0 aht;
  Adafruit_BMP280 bmp;
  bool bmpOk = false;
};
