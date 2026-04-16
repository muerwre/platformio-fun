#pragma once
#include <Wire.h>
#include <Adafruit_SCD30.h>
#include "reporter.h"

#define RETRIES_COUNT 5
#define MEASURE_INTERVAL 10  // seconds
#define WARMUP_INTERVAL (MEASURE_INTERVAL * 1.25)

struct SCD30Reading
{
  float temperature;
  float humidity;
  float co2;
};

class TemperatureReporter : public Reporter
{
public:
  TemperatureReporter() {}

  int initSensor()
  {
    // Wire must already be initialized by the caller (Wire.begin in setup())
    // default speed (100kHz) causes issues with the SCD30, so use 10kHz
    // https://forums.adafruit.com/viewtopic.php?t=217459
    Wire.setClock(10000L);

    int retries = RETRIES_COUNT;
    while (!scd30.begin() && retries > 0)
    {
      Serial.printf("Failed to find SCD30 chip, retrying... (%d left)\n", retries);
      delay(1000);
      retries--;
    }

    if (retries <= 0)
    {
      Serial.println("Failed to find SCD30 chip after retries");
      return 1;
    }

    scd30.setMeasurementInterval(2);
    sensor_available = true;
    Serial.printf("SCD30 found! Interval: %ds\n", scd30.getMeasurementInterval());
    return 0;
  }

  // Reads a valid measurement into `out`. Returns 0 on success.
  int measure(SCD30Reading &out)
  {
    if (!sensor_available)
    {
      Serial.println("SCD30 not available");
      return 1;
    }

    Serial.print("Waiting for SCD30 data");
    unsigned long deadline = millis() + (unsigned long)(WARMUP_INTERVAL * 1e3);
    while (!scd30.dataReady() && millis() < deadline)
    {
      Serial.print(".");
      delay(300);
    }
    Serial.println();

    if (!scd30.dataReady())
    {
      Serial.println("SCD30 data not ready, skipping");
      return 1;
    }

    deadline = millis() + (unsigned long)(WARMUP_INTERVAL * 1e3);
    while ((!scd30.read() || scd30.CO2 == 0 || !isfinite(scd30.CO2)) && millis() < deadline)
    {
      delay(1000);
    }

    if (scd30.CO2 == 0 || !isfinite(scd30.CO2))
    {
      Serial.println("SCD30 invalid reading, skipping");
      return 1;
    }

    out.temperature = scd30.temperature;
    out.humidity = scd30.relative_humidity;
    out.co2 = scd30.CO2;

    scd30.setMeasurementInterval(MEASURE_INTERVAL);
    scd30.startContinuousMeasurement();

    return 0;
  }

  int report() override { return 0; }

private:
  Adafruit_SCD30 scd30;
  bool sensor_available = false;
};
