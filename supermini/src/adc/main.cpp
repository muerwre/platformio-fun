#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include "secrets.h"

// Voltage divider: 100k / 100k between VIN and GND, midpoint to ADC pin
// Divider ratio = (R1 + R2) / R2 = 2.0
#define ADC_PIN 0
#define VOLTAGE_DIVIDER_RATIO 2.039
#define READ_INTERVAL_MS 1000

const char *ssid = WIFI_SSID;
const char *password = WIFI_PASS;
const char *mqtt_broker = MQTT_SERVER;
const int mqtt_port = MQTT_PORT;
const char *mqtt_username = MQTT_USER;
const char *mqtt_password = MQTT_PASS;
const char *mqtt_client_id = "ESP32C6_ADC";
const char *mqtt_topic = MQTT_TOPIC;
const unsigned long connect_timeout = 30e3;

WiFiClient espClient;
PubSubClient mqtt_client(espClient);

void connectWiFi()
{
  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < connect_timeout)
  {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("\nWiFi connection failed!");
    return;
  }

  Serial.printf("\nWiFi connected, IP: %s\n", WiFi.localIP().toString().c_str());
}

void connectMQTT()
{
  unsigned long start = millis();

  while (!mqtt_client.connected() && millis() - start < connect_timeout)
  {
    Serial.println("Connecting to MQTT...");

    if (mqtt_client.connect(mqtt_client_id, mqtt_username, mqtt_password))
    {
      Serial.println("MQTT connected!");
      return;
    }

    Serial.printf("MQTT failed, rc=%d, retrying...\n", mqtt_client.state());
    delay(1000);
  }
}

void setup()
{
  Serial.begin(115200);
  analogReadResolution(12);
  pinMode(ADC_PIN, INPUT);

  delay(1000);
  Serial.println("ADC voltage measurement started");

  connectWiFi();
  mqtt_client.setServer(mqtt_broker, mqtt_port);
  connectMQTT();
}

void loop()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    connectWiFi();
  }

  if (!mqtt_client.connected())
  {
    connectMQTT();
  }

  mqtt_client.loop();

  int raw = analogRead(ADC_PIN);
  int mv = analogReadMilliVolts(ADC_PIN);
  float voltage = (mv / 1000.0) * VOLTAGE_DIVIDER_RATIO;

  Serial.printf("ADC: %d\tmV: %d\tVoltage: %.2f V\n", raw, mv, voltage);

  String adcTopic = String(mqtt_topic) + "/adc";
  String mvTopic = String(mqtt_topic) + "/mv";
  String voltageTopic = String(mqtt_topic) + "/voltage";

  mqtt_client.publish(adcTopic.c_str(), String(raw).c_str(), false);
  mqtt_client.loop();
  mqtt_client.publish(mvTopic.c_str(), String(mv).c_str(), false);
  mqtt_client.loop();
  mqtt_client.publish(voltageTopic.c_str(), String(voltage, 2).c_str(), false);
  mqtt_client.loop();

  delay(READ_INTERVAL_MS);
}
