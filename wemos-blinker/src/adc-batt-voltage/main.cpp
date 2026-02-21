#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include "env.h"

// WiFi credentials
const char *ssid = WIFI_SSID;
const char *password = WIFI_PASS;

// MQTT Broker settings
const char *mqtt_broker = MQTT_SERVER; // probably should be mdns?
const int mqtt_port = MQTT_PORT;
const char *mqtt_username = MQTT_USER;
const char *mqtt_password = MQTT_PASS;
const char *mqtt_client_id = "ESP8266_DHT11";
const char *mqtt_topic = MQTT_TOPIC;

// MAX ADC IS AROUND 965 when wiring through 50kOhm resistor to 4.2V, adjust as necessary

WiFiClient espClient;
PubSubClient mqtt_client(espClient);

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
  Serial.begin(115200);
  mqtt_client.setServer(mqtt_broker, mqtt_port);
}

void loop()
{
  static int min_adc_value = 1023;
  static int max_adc_value = 0;

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

  /**
   * MEASURE
   */
  unsigned long uptimeMinutes = millis() / 60000;
  int adcValue = analogRead(A0);
  int adcValues[] = {adcValue};

  // measure for 10 seconds and take the minimum value to avoid spikes, adjust as necessary
  for (int i = 1; i < 10; i++)
  {
    delay(1000);
    int currentAdcValue = analogRead(A0);
    adcValue = min(adcValue, currentAdcValue);
    adcValues[i] = currentAdcValue;
  }

  if (adcValue > max_adc_value)
  {
    max_adc_value = adcValue;
  }

  if (adcValue < min_adc_value)
  {
    min_adc_value = adcValue;
  }

  float voltage = (adcValue / 1023.0) * 3.5; // 4.2V - 0.7V on diode drop, adjust as necessary

  // Print the voltage to the Serial Monitor
  Serial.printf(
      "Battery Voltage: %.2f V, ADC Value: %d, Min ADC Value: %d, Max ADC Value: %d, Uptime: %lu minutes\n",
      voltage, adcValue, min_adc_value, max_adc_value, uptimeMinutes);
  Serial.println("--- Readings: " + String(adcValues[0]) + ", " + String(adcValues[1]) + ", " + String(adcValues[2]) + ", " + String(adcValues[3]) + ", " + String(adcValues[4]) + ", " + String(adcValues[5]) + ", " + String(adcValues[6]) + ", " + String(adcValues[7]) + ", " + String(adcValues[8]) + ", " + String(adcValues[9]));

  /** ----------------- BEGINING WORK -------------------------------------- */
  String adcTopic = String(mqtt_topic) + "/adcValue";
  mqtt_client.publish(adcTopic.c_str(), String(adcValue).c_str(), true);
  mqtt_client.loop();
  delay(50);
  yield();

  String voltageTopic = String(mqtt_topic) + "/voltage";
  mqtt_client.publish(voltageTopic.c_str(), String(voltage, 2).c_str(), true);
  mqtt_client.loop();
  delay(50);
  yield();

  String minAdcTopic = String(mqtt_topic) + "/minAdcValue";
  mqtt_client.publish(minAdcTopic.c_str(), String(min_adc_value).c_str(), true);
  mqtt_client.loop();
  delay(50);
  yield();

  String maxAdcTopic = String(mqtt_topic) + "/maxAdcValue";
  mqtt_client.publish(maxAdcTopic.c_str(), String(max_adc_value).c_str(), true);
  mqtt_client.loop();
  delay(50);
  yield();

  String uptimeTopic = String(mqtt_topic) + "/uptime";
  mqtt_client.publish(uptimeTopic.c_str(), String(uptimeMinutes).c_str(), true);
  mqtt_client.loop();
  delay(50);
  yield();
}