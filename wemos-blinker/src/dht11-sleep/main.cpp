#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <time.h>
#include "DHT.h"
#include "env.h"

// settings
#define SLEEP_DURATION_SECONDS 600 // 10 minutes
#define RETRY_DURATION_SECONDS 10  // not more than 20 seconds!

// WIRING: VCC to 3.3V, GND to GND, DATA to D2
#define DHT11_PIN D2

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

// NTP settings
const char *ntp_server = "pool.ntp.org";
const long gmt_offset_sec = 7;     // GMT offset in seconds (adjust for your timezone)
const int daylight_offset_sec = 0; // Daylight saving offset in seconds

DHT dht11(DHT11_PIN, DHT11);
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

  // Configure NTP
  // configTime(gmt_offset_sec, daylight_offset_sec, ntp_server);
  // Serial.println("Waiting for NTP time sync...");

  // time_t now = time(nullptr);
  // int retries = 0;
  // while (now < 8 * 3600 * 2 && retries < 20)
  // {
  //   delay(500);
  //   Serial.print(".");
  //   now = time(nullptr);
  //   retries++;
  // }
  // Serial.println();

  // if (now >= 8 * 3600 * 2)
  // {
  //   Serial.println("NTP time synced!");
  //   Serial.print("Current time: ");
  //   Serial.println(ctime(&now));
  // }
  // else
  // {
  //   Serial.println("NTP sync failed, continuing with system time");
  // }
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
  delay(10);
  Serial.println("\nStarting sensor");
  dht11.begin(); // initialize the sensor
  connectWiFi();
  mqtt_client.setServer(mqtt_broker, mqtt_port);
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

  // read humidity
  float humi = dht11.readHumidity();
  // read temperature as Celsius
  float tempC = dht11.readTemperature();

  if (isnan(humi) || isnan(tempC))
  {
    Serial.println("Failed to read from DHT11 sensor!");
    ESP.deepSleep(RETRY_DURATION_SECONDS * 1e6);

    return;
  }

  Serial.printf("Humidity: %.2f%%, Temperature: %.2f°C\n", humi, tempC);

  // Get current time
  // time_t now = time(nullptr);
  // struct tm *timeinfo = localtime(&now);
  // char timeStr[64];
  // strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", timeinfo);

  String tempTopic = String(mqtt_topic) + "/temperature";
  String humiTopic = String(mqtt_topic) + "/humidity";

  if (mqtt_client.publish(tempTopic.c_str(), String(tempC, 2).c_str()))
  {
    Serial.printf("Published temperature to MQTT: %.2f°C --> %s\n", tempC, tempTopic.c_str());
  }
  else
  {
    Serial.println("Failed to publish temperature to MQTT");
    ESP.deepSleep(RETRY_DURATION_SECONDS * 1e6);
    return;
  }

  mqtt_client.loop();
  delay(50);

  if (mqtt_client.publish(humiTopic.c_str(), String(humi, 2).c_str()))
  {
    Serial.printf("Published humidity to MQTT: %.2f%% --> %s\n", humi, humiTopic.c_str());
  }
  else
  {
    Serial.println("Failed to publish humidity to MQTT");
    ESP.deepSleep(RETRY_DURATION_SECONDS * 1e6);
    return;
  }

  mqtt_client.loop();

  Serial.printf("Work's done, going to sleep for %d seconds...\n", SLEEP_DURATION_SECONDS);

  ESP.deepSleep(SLEEP_DURATION_SECONDS * 1e6);
}