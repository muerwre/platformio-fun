#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <FastLED.h>
#include "LEDControl.h"
#include "secrets.h"

#define LED_PIN D3
#define MATRIX_WIDTH 16
#define MATRIX_HEIGHT 16

// WiFi credentials
const char *ssid = WIFI_SSID;
const char *password = WIFI_PASS;

// MQTT Broker settings
const char *mqtt_broker = MQTT_SERVER;
const int mqtt_port = MQTT_PORT;
const char *mqtt_username = MQTT_USER;
const char *mqtt_password = MQTT_PASS;
const char *mqtt_client_id = "ESP8266_WS2812";
const char *mqtt_topic_base = MQTT_TOPIC;

// MQTT Topics
String topic_status;
String topic_command;
String topic_brightness_status;
String topic_brightness_set;
String topic_rgb_status;
String topic_rgb_set;

LEDControl<LED_PIN, MATRIX_WIDTH, MATRIX_HEIGHT> ledControl;
WiFiClient espClient;
PubSubClient mqtt_client(espClient);

unsigned long lastLEDUpdate = 0;
const unsigned long LED_UPDATE_INTERVAL = 100; // Update LEDs every 100ms

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
}

void publishStatus()
{
  // Publish state
  String state = ledControl.getState() ? "ON" : "OFF";
  mqtt_client.publish(topic_status.c_str(), state.c_str(), true);

  // Publish brightness
  char brightnessStr[10];
  snprintf(brightnessStr, sizeof(brightnessStr), "%d", ledControl.getBrightness());
  mqtt_client.publish(topic_brightness_status.c_str(), brightnessStr, true);

  // Publish RGB
  CRGB color = ledControl.getColor();
  char rgbStr[20];
  snprintf(rgbStr, sizeof(rgbStr), "%d,%d,%d", color.r, color.g, color.b);
  mqtt_client.publish(topic_rgb_status.c_str(), rgbStr, true);

  Serial.printf("Published: state=%s, brightness=%d, rgb=%d,%d,%d\n",
                state.c_str(), ledControl.getBrightness(), color.r, color.g, color.b);
}

void mqttCallback(char *topic, byte *payload, unsigned int length)
{
  String message;
  for (unsigned int i = 0; i < length; i++)
  {
    message += (char)payload[i];
  }

  Serial.printf("Message arrived [%s]: %s\n", topic, message.c_str());

  // Handle switch command
  if (String(topic) == topic_command)
  {
    if (message == "ON")
    {
      ledControl.turnOn();
    }
    else if (message == "OFF")
    {
      ledControl.turnOff();
    }
    publishStatus();
  }
  // Handle brightness set
  else if (String(topic) == topic_brightness_set)
  {
    int brightness = message.toInt();
    if (brightness >= 0 && brightness <= 255)
    {
      ledControl.setBrightness(brightness);
      publishStatus();
    }
  }
  // Handle RGB set
  else if (String(topic) == topic_rgb_set)
  {
    int r, g, b;
    if (sscanf(message.c_str(), "%d,%d,%d", &r, &g, &b) == 3)
    {
      if (r >= 0 && r <= 255 && g >= 0 && g <= 255 && b >= 0 && b <= 255)
      {
        Serial.printf("Setting RGB: %d,%d,%d\n", r, g, b);
        ledControl.setRGB(r, g, b);
        publishStatus();
      }
    }
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

      // Subscribe to command topics
      mqtt_client.subscribe(topic_command.c_str());
      mqtt_client.subscribe(topic_brightness_set.c_str());
      mqtt_client.subscribe(topic_rgb_set.c_str());

      Serial.printf("Subscribed to:\n  %s\n  %s\n  %s\n",
                    topic_command.c_str(),
                    topic_brightness_set.c_str(),
                    topic_rgb_set.c_str());

      // Don't publish initial status - wait for commands from Home Assistant
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
  Serial.println("\nWS2812 MQTT Light Starting...");

  // Initialize LED control
  ledControl.begin<WS2812>();
  // Start with LED off, wait for MQTT commands
  ledControl.turnOff();

  // Build topic strings
  String base = String(mqtt_topic_base);
  if (!base.endsWith("/"))
    base += "/";

  topic_status = base + "status";
  topic_command = base + "switch";
  topic_brightness_status = base + "brightness/status";
  topic_brightness_set = base + "brightness/set";
  topic_rgb_status = base + "rgb/status";
  topic_rgb_set = base + "rgb/set";

  // Connect to WiFi
  connectWiFi();

  // Setup MQTT
  mqtt_client.setServer(mqtt_broker, mqtt_port);
  mqtt_client.setCallback(mqttCallback);
}

void loop()
{
  // Ensure WiFi connection
  if (WiFi.status() != WL_CONNECTED)
  {
    connectWiFi();
  }

  yield();

  // Ensure MQTT connection
  if (!mqtt_client.connected())
  {
    connectMQTT();
  }
  mqtt_client.loop();

  yield();

  // Periodically refresh LEDs
  unsigned long currentMillis = millis();
  if (currentMillis - lastLEDUpdate >= LED_UPDATE_INTERVAL)
  {
    lastLEDUpdate = currentMillis;
    ledControl.refresh();
  }
}