#pragma once
#include <math.h>
#include <stdint.h>

// A device-agnostic telemetry sample, mirroring Meshtastic's EnvironmentMetrics
// field-for-field. Each member's comment is the EnvironmentMetrics field number
// it serialises to (see MeshNode::sendTelemetry).
//
// Float fields default to NAN and integer fields to UNSET_U32, both meaning "not
// present" — a device fills in only what its sensors measure and the rest are
// omitted from the wire packet. So a bare-bones node sets two fields; a weather
// station can set twenty, with no change to the library.
//
// Note: there is no dedicated battery field on the node-data path here — voltage
// (field 5) is for measured supply voltage. Some devices deliberately report
// their battery in `lux` (field 9) instead, to avoid colliding with the parent
// node's own voltage; that's an app choice, made in the telemetry provider.
struct Telemetry
{
  static constexpr uint32_t UNSET_U32 = 0xFFFFFFFF;

  float temperature = NAN;         // °C        (field 1)
  float humidity = NAN;            // %RH       (2  relative_humidity)
  float pressure = NAN;            // hPa       (3  barometric_pressure)
  float gasResistance = NAN;       // MΩ        (4  gas_resistance)
  float voltage = NAN;             // V         (5)
  float current = NAN;             // mA        (6)
  uint32_t iaq = UNSET_U32;        // 0..500    (7  indoor air quality)
  float distance = NAN;            // mm        (8)
  float lux = NAN;                 // lx        (9)
  float whiteLux = NAN;            // lx        (10 white_lux)
  float irLux = NAN;               // lx        (11 ir_lux)
  float uvLux = NAN;               // lx        (12 uv_lux)
  uint32_t windDirection = UNSET_U32; // degrees (13 wind_direction)
  float windSpeed = NAN;           // m/s       (14 wind_speed)
  float weight = NAN;              // kg        (15)
  float windGust = NAN;            // m/s       (16 wind_gust)
  float windLull = NAN;            // m/s       (17 wind_lull)
  float radiation = NAN;           // µR/h      (18)
  float rainfall1h = NAN;          // mm        (19 rainfall_1h)
  float rainfall24h = NAN;         // mm        (20 rainfall_24h)
  uint32_t soilMoisture = UNSET_U32; // %       (21 soil_moisture)
  float soilTemperature = NAN;     // °C        (22 soil_temperature)
};

// Signature for a device's telemetry provider. Fill `out` and return true on a
// good read; return false to signal a sensor failure (the connector then reports
// "sensor error" on a plain timer wake instead of sending bad data).
//
// IMPORTANT: this runs BEFORE the BLE radio starts, because I2C and NimBLE clash
// on the ESP32-C6 if the bus is still live when the radio initialises. Do all
// sensor I/O here and release the bus (Wire.end()) before returning.
typedef bool (*TelemetryProvider)(Telemetry &out);
