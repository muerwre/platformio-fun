#include <Arduino.h>
#include <mesh_connector.h>
#include "secrets.h"
#include "sensors.h"

// --- Pin configuration ---
#define LED_PIN 8    // onboard WS2812
#define WAKEUP_PIN 5 // PIR motion sensor (GPIO0-7 wakeup range; GPIO5 is a strapping pin)
#define SDA_PIN 20   // AHT20 + BMP280 I2C
#define SCL_PIN 19

MeshConnector mesh(LED_PIN);
Sensors sensors(SDA_PIN, SCL_PIN);

// Telemetry provider: read AHT20 + BMP280 + battery and map onto the wire fields.
// Runs before BLE starts, so we own I2C here and release it (Wire.end()) before
// returning — NimBLE and the I2C bus clash on the ESP32-C6 otherwise.
bool readTelemetry(Telemetry &t)
{
  bool ok = sensors.begin();
  SensorReading r;
  if (ok)
    r = sensors.read();
  Wire.end();
  if (!ok)
    return false;

  t.temperature = r.temperature;
  t.humidity = r.humidity;
  t.pressure = r.pressure;
  t.lux = r.voltage; // battery reported as lux (9) to avoid colliding with the parent node's voltage
  return true;
}

void setup()
{
  Serial.begin(115200);
  delay(200);

  mesh.setNode(MESH_BLE_MAC, MESH_BLE_PIN);
  mesh.setDestination(MESH_DEST_ID, MESH_CHANNEL);
  mesh.setHopLimit(MESH_HOP_LIMIT);
  mesh.setTelemetryInterval(MESH_SEND_INTERVAL_MIN * 60);
  mesh.setWakeUpMode(HIGH); // PIR drives the pin HIGH on motion
  mesh.addWakeupTrigger(WAKEUP_PIN, "Main Entrance Open", 5 /* min cooldown */);
  mesh.onTelemetry(readTelemetry);

  mesh.run(); // wake -> decide -> connect -> send -> deep-sleep; never returns
}

void loop() {}
