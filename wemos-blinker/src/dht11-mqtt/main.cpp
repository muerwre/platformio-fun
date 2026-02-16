#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <time.h>
#include "DHT.h"

// WIRING: VCC to 3.3V, GND to GND, DATA to D2
#define DHT11_PIN D2

// WiFi credentials
const char *ssid = "einsam.ru";
const char *password = "sepukko1337825";

// MQTT Broker settings
const char *mqtt_broker = "192.168.1.146"; // probably should be mdns?
const int mqtt_port = 1883;
const char *mqtt_username = "user";
const char *mqtt_password = "password";
const char *mqtt_client_id = "ESP8266_DHT11";
const char *mqtt_topic = "metrics/dht11";

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

  Serial.println("\nWiFi connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // Configure NTP
  configTime(gmt_offset_sec, daylight_offset_sec, ntp_server);
  Serial.println("Waiting for NTP time sync...");

  time_t now = time(nullptr);
  int retries = 0;
  while (now < 8 * 3600 * 2 && retries < 20)
  {
    delay(500);
    Serial.print(".");
    now = time(nullptr);
    retries++;
  }
  Serial.println();

  if (now >= 8 * 3600 * 2)
  {
    Serial.println("NTP time synced!");
    Serial.print("Current time: ");
    Serial.println(ctime(&now));
  }
  else
  {
    Serial.println("NTP sync failed, continuing with system time");
  }
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
  Serial.println("\nDHT11 MQTT Sensor Starting...");

  dht11.begin(); // initialize the sensor

  connectWiFi();

  mqtt_client.setServer(mqtt_broker, mqtt_port);
}

void loop()
{
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

  // check if any reads failed
  if (isnan(humi) || isnan(tempC))
  {
    Serial.println("Failed to read from DHT11 sensor!");
  }
  else
  {
    Serial.printf("Humidity: %.2f%%, Temperature: %.2f°C\n", humi, tempC);

    // Get current time
    time_t now = time(nullptr);
    struct tm *timeinfo = localtime(&now);
    char timeStr[64];
    strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", timeinfo);

    // Format JSON message with sprintf
    char jsonBuffer[256];
    snprintf(jsonBuffer, sizeof(jsonBuffer),
             "{\"time\":\"%s\",\"temp\":%.2f,\"humidity\":%.2f}",
             timeStr, tempC, humi);

    // Publish to MQTT
    if (mqtt_client.publish(mqtt_topic, jsonBuffer))
    {
      Serial.printf("Published: %s\n", jsonBuffer);
    }
    else
    {
      Serial.println("Failed to publish");
    }
  }

  // Wait 60 seconds between readings
  for (int i = 0; i < 12; ++i)
  {
    delay(5000);
    mqtt_client.loop();
    yield();
  }
}