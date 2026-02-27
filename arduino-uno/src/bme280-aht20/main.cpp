#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h>
#include <Adafruit_AHTX0.h>

#define SEALEVELPRESSURE_HPA (1013.25)

/**
 * Wiring:
 *  VDD -> 3.3V
 *  GND -> GND
 *  SCL -> A5
 *  SDA -> A4
 */
Adafruit_Sensor *aht_humidity, *aht_temp, *bmp_temp, *bmp_press;
Adafruit_BMP280 bmp;
Adafruit_AHTX0 aht;

void setup()
{
  Serial.begin(115200);
  if (!bmp.begin())
  {
    Serial.println("Could not find a valid BMP280 sensor, check wiring!");
    while (1)
      ;
  }

  Serial.println("BMP280 Found!");
  bmp_temp = bmp.getTemperatureSensor();
  bmp_temp->printSensorDetails();
  bmp_press = bmp.getPressureSensor();
  bmp_press->printSensorDetails();

  if (!aht.begin())
  {
    Serial.println("Failed to find AHT10/AHT20 chip");
    while (1)
    {
      delay(10);
    }
  }

  Serial.println("AHT10/AHT20 Found!");
  aht_temp = aht.getTemperatureSensor();
  aht_temp->printSensorDetails();

  aht_humidity = aht.getHumiditySensor();
  aht_humidity->printSensorDetails();
}

void loop()
{
  sensors_event_t humidity;
  sensors_event_t temp;

  aht_temp->getEvent(&temp);
  aht_humidity->getEvent(&humidity);

  Serial.print("Temperature, BMP280: ");
  Serial.print(bmp.readTemperature());
  Serial.print("*C, AHT20: ");
  Serial.print(temp.temperature);
  Serial.println("*C");

  Serial.print("Pressure = ");
  Serial.print(bmp.readPressure() / 100.0F);
  Serial.print(" hPa, ");
  Serial.print(bmp.readPressure() * 0.00750062); // Convert Pa to mmHg
  Serial.println(" mmHg");

  Serial.print("Humidity: ");
  Serial.print(humidity.relative_humidity);
  Serial.println(" % rH");
  Serial.println();

  delay(10000);
}