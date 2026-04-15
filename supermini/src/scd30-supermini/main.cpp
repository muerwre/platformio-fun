#include <Arduino.h>
#include <PubSubClient.h>
#include <time.h>
#include "secrets.h"
#include "mqtt_control.h"
#include "voltage_reporter.h"
#include "temp_reporter.h"
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <Adafruit_NeoPixel.h>

#define LED_PIN 8
Adafruit_NeoPixel led(1, LED_PIN, NEO_GRB + NEO_KHZ800);

// settings
#define SLEEP_DURATION_SECONDS (20 * 60) // (seconds) sleep duration between successful measurements
#define RETRY_DURATION_SECONDS (2 * 60)  // (seconds) sleep duration between failed measurements

// WIRING: VCC to 3.3V, GND to GND
#define SCL_PIN SCL1 // 7
#define SDA_PIN SDA1 // 6

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

// WiFi and MQTT connection settings
const unsigned long connect_timeout = 30e3; // WiFi connection timeout in milliseconds

// Voltage level measurement settings
const bool enable_voltage_reporting = true; // Set to true to enable voltage measurement
const int adc_pin = A0;                     // ADC pin for voltage measurement
const int mosfet_control_pin = -1;          // GPIO pin to open MOSFET between BATT and ADC (A0), -1 for disabled

WiFiClient espClient;
PubSubClient mqtt_client(espClient);
TemperatureReporter *tempReporter = nullptr;
MqttControl mqttControl(mqtt_client, MQTT_TOPIC, SLEEP_DURATION_SECONDS);

void connectWiFi()
{
  pinMode(adc_pin, INPUT);
  int value = analogRead(adc_pin);
  Serial.printf("ADC: %d", value);
  Serial.println("\nConnecting to WiFi...");
  WiFi.begin(ssid, password);

  unsigned long startAttemptTime = millis();

  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < connect_timeout)
  {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("\nFailed to connect to WiFi - going to sleep and retrying...");
    if (RETRY_DURATION_SECONDS > 0)
    {
      mqtt_client.disconnect();
      ESP.deepSleep(RETRY_DURATION_SECONDS * 1e6);
    }
    return;
  }

  Serial.printf("\nWiFi connected, IP address: %s\n", WiFi.localIP().toString().c_str());
}

void connectMQTT()
{
  unsigned long startAttemptTime = millis();

  while (!mqtt_client.connected() && millis() - startAttemptTime < connect_timeout)
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
      Serial.println(" - retrying");
      delay(1000);
      yield();
    }
  }

  if (!mqtt_client.connected())
  {
    Serial.println("Failed to connect to MQTT broker - going to sleep and retrying...");
    if (RETRY_DURATION_SECONDS > 0)
    {
      ESP.deepSleep(RETRY_DURATION_SECONDS * 1e6);
    }
    return;
  }
}

void setup()
{
  led.begin();
  led.setBrightness(10);
  led.setPixelColor(0, led.Color(0, 0, 255));
  led.show();
  delay(100);
  led.setPixelColor(0, led.Color(0, 0, 0));
  led.show();

  Serial.begin(74880);
  Wire.begin(SDA_PIN, SCL_PIN);
  tempReporter = new TemperatureReporter(mqtt_client, SCL_PIN, SDA_PIN, MQTT_TOPIC);
  Serial.println("\nStarting sensor");

  if (tempReporter->initSensor() > 0)
  {
    Serial.printf("Error initializing sensor, going to sleep for %d seconds...\n", RETRY_DURATION_SECONDS);
    if (RETRY_DURATION_SECONDS > 0)
    {
      mqtt_client.disconnect();
      ESP.deepSleep(RETRY_DURATION_SECONDS * 1e6);
    }
  }

  connectWiFi();
  mqtt_client.setServer(mqtt_broker, mqtt_port);
  mqtt_client.setCallback([](char *topic, byte *payload, unsigned int length)
                          { mqttControl.callback(topic, payload, length); });
  connectMQTT();
  mqttControl.readUpdateInterval();
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

  if (enable_voltage_reporting)
  {
    static VoltageReporter voltageReporter(mqtt_client, adc_pin, mosfet_control_pin, mqtt_topic);
    voltageReporter.report();
    yield();
  }

  if (result != 0)
  {
    Serial.printf("Work's NOT done, going to sleep for %d seconds...\n", RETRY_DURATION_SECONDS);
    mqtt_client.loop();
    delay(200);
    if (RETRY_DURATION_SECONDS > 0)
    {
      mqtt_client.disconnect();
      ESP.deepSleep(RETRY_DURATION_SECONDS * 1e6);
    }
    return;
  }

  long sleepSeconds = mqttControl.getSleepDurationSeconds();
  Serial.printf("Work's done, going to sleep for %ld seconds...\n", sleepSeconds);

  mqtt_client.loop();
  delay(200);

  if (sleepSeconds > 0)
  {
    mqtt_client.disconnect();
    ESP.deepSleep(sleepSeconds * 1e6);
  }
}