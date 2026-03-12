#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <time.h>
#include "secrets.h"
#include "voltage_reporter.h"
#include "temp_reporter.h"
#include "ntp_reporter.h"

// settings
#define SLEEP_DURATION_SECONDS (20 * 60) // 20 minutes
#define RETRY_DURATION_SECONDS 10        // not more than 20 seconds!

// WIRING: VCC to 3.3V, GND to GND, DATA to D2
#define SCL_PIN D1
#define SDA_PIN D2

// WiFi credentials
const char *ssid = WIFI_SSID;
const char *password = WIFI_PASS;

// MQTT Broker settings
const char *mqtt_broker = MQTT_SERVER; // probably should be mdns?
const int mqtt_port = MQTT_PORT;
const char *mqtt_username = MQTT_USER;
const char *mqtt_password = MQTT_PASS;
const char *mqtt_client_id = "ESP8266_AHT20";
const char *mqtt_topic = MQTT_TOPIC;

// NTP settings
const char *ntp_server = "pool.ntp.org";
const long gmt_offset_seconds = 7 * 3600; // GMT offset in seconds (adjust for your timezone)
const int daylight_offset_seconds = 0;    // Daylight saving offset in seconds
const bool enable_ntp_reporting = false;  // Set to true to enable NTP time reporting to MQTT

// Voltage level measurement settings
const bool enable_voltage_reporting = true; // Set to true to enable voltage measurement
const int adc_pin = A0;                     // ADC pin for voltage measurement
const int mosfet_control_pin = D5;          // GPIO pin to open MOSFET between BATT and ADC (A0)

WiFiClient espClient;
PubSubClient mqtt_client(espClient);
TemperatureReporter *tempReporter = nullptr;

void connectWiFi()
{
  Serial.println("\nConnecting to WiFi...");
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.printf("\nWiFi connected, IP address: %s\n", WiFi.localIP().toString().c_str());
}

void connectMQTT()
{
  while (!mqtt_client.connected())
  {
    Serial.println("Connecting to MQTT broker...");

    if (mqtt_client.connect(mqtt_client_id, mqtt_username, mqtt_password))
    {
      Serial.println("MQTT connected!");
    }
    else
    {
      Serial.print("MQTT connection failed, rc=");
      Serial.print(mqtt_client.state());
      Serial.println(" - retrying in 5 seconds");
      delay(5000);
      yield();
    }
  }
}

void setup()
{
  Wire.begin(D2, D1);
  Serial.begin(74880);
  delay(10);
  Serial.println("\nStarting sensor");
  connectWiFi();
  mqtt_client.setServer(mqtt_broker, mqtt_port);
  tempReporter = new TemperatureReporter(mqtt_client, SCL_PIN, SDA_PIN, MQTT_TOPIC);
  tempReporter->initSensor();
}

void loop()
{
  Serial.println("Starting work");

  // Ensure WiFi and MQTT connections
  if (WiFi.status() != WL_CONNECTED)
  {
    connectWiFi();
  }

  yield();

  if (!mqtt_client.connected())
  {
    connectMQTT();
  }
  mqtt_client.loop();
  yield();

  int result = tempReporter->report();
  yield();

  if (result != 0)
  {
    ESP.deepSleep(RETRY_DURATION_SECONDS * 1e6);
    return;
  }

  if (enable_voltage_reporting)
  {
    static VoltageReporter voltageReporter(mqtt_client, adc_pin, mosfet_control_pin, mqtt_topic);
    voltageReporter.report();
    yield();
  }

  if (enable_ntp_reporting)
  {
    static NTPReporter ntpReporter(mqtt_client, mqtt_topic, gmt_offset_seconds, daylight_offset_seconds, ntp_server);
    ntpReporter.report();
    yield();
  }

  Serial.printf("Work's done, going to sleep for %d seconds...\n", SLEEP_DURATION_SECONDS);

  ESP.deepSleep(SLEEP_DURATION_SECONDS * 1e6);
}