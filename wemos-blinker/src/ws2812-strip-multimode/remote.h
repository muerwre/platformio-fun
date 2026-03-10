#pragma once

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <FastLED.h>

class RemoteControl
{
public:
  RemoteControl(
      const char *wifiSsid, const char *wifiPass,
      const char *mqttServer, int mqttPort,
      const char *mqttUser, const char *mqttPass,
      const char *clientId, const char *topicBase)
      : wifiSsid(wifiSsid), wifiPass(wifiPass),
        mqttServer(mqttServer), mqttPort(mqttPort),
        mqttUser(mqttUser), mqttPass(mqttPass),
        clientId(clientId), topicBase(topicBase),
        mqtt(espClient),
        color(CRGB::Black), status(false), brightness(128)
  {
    instance = this;
  }

  void setup()
  {
    String base = String(topicBase);
    if (!base.endsWith("/"))
      base += "/";

    topicStatus = base + "status";
    topicCommand = base + "switch";
    topicBrightSet = base + "brightness/set";
    topicBrightStatus = base + "brightness/status";
    topicColorSet = base + "rgb/set";
    topicColorStatus = base + "rgb/status";
    topicModeSet = base + "mode/set";
    topicModeStatus = base + "mode/status";

    connectWiFi();

    mqtt.setServer(mqttServer, mqttPort);
    mqtt.setCallback(mqttCallback);
    connectMQTT();
  }

  void loop()
  {
    if (WiFi.status() != WL_CONNECTED)
      connectWiFi();

    yield();

    if (!mqtt.connected())
      connectMQTT();

    mqtt.loop();
  }

  CRGB getColor() const { return color; }
  bool getStatus() const { return status; }
  uint8_t getBrightness() const { return brightness; }
  int getMode() const { return mode; }
  bool isModeChanged() { return modeChanged; }
  void clearModeChanged() { modeChanged = false; }
  bool isBrightnessChanged() { return brightnessChanged; }
  void clearBrightnessChanged() { brightnessChanged = false; }
  bool isStatusChanged() { return statusChanged; }
  void clearStatusChanged() { statusChanged = false; }

  void sendColor(CRGB c)
  {
    char buf[20];
    snprintf(buf, sizeof(buf), "%d,%d,%d", c.r, c.g, c.b);
    mqtt.publish(topicColorStatus.c_str(), buf, true);
  }

  void sendStatus(bool s)
  {
    mqtt.publish(topicStatus.c_str(), s ? "ON" : "OFF", true);
  }

  void sendBrightness(uint8_t b)
  {
    char buf[10];
    snprintf(buf, sizeof(buf), "%d", b);
    mqtt.publish(topicBrightStatus.c_str(), buf, true);
  }

  void sendMode(int m)
  {
    mqtt.publish(topicModeStatus.c_str(), String(m).c_str(), true);
  }

private:
  const char *wifiSsid, *wifiPass;
  const char *mqttServer;
  int mqttPort;
  const char *mqttUser, *mqttPass, *clientId, *topicBase;

  String topicStatus, topicCommand;
  String topicBrightSet, topicBrightStatus;
  String topicColorSet, topicColorStatus;
  String topicModeSet, topicModeStatus;

  CRGB color;
  bool status;
  uint8_t brightness;
  int mode = 0;
  bool modeChanged = false;
  bool brightnessChanged = false;
  bool statusChanged = false;

  WiFiClient espClient;
  PubSubClient mqtt;

  inline static RemoteControl *instance = nullptr;

  static void mqttCallback(char *topic, byte *payload, unsigned int length)
  {
    if (instance)
      instance->handleMessage(topic, payload, length);
  }

  void handleMessage(const char *topic, const byte *payload, unsigned int length)
  {
    String message;
    for (unsigned int i = 0; i < length; i++)
      message += (char)payload[i];

    Serial.printf("MQTT [%s]: %s\n", topic, message.c_str());

    if (String(topic) == topicCommand)
    {
      status = (message == "ON");
      statusChanged = true;
      sendStatus(status);
    }
    else if (String(topic) == topicBrightSet)
    {
      int b = message.toInt();
      if (b >= 0 && b <= 255)
      {
        brightness = (uint8_t)b;
        brightnessChanged = true;
        sendBrightness(brightness);
      }
    }
    else if (String(topic) == topicColorSet)
    {
      int r, g, b;
      if (sscanf(message.c_str(), "%d,%d,%d", &r, &g, &b) == 3)
        if (r >= 0 && r <= 255 && g >= 0 && g <= 255 && b >= 0 && b <= 255)
        {
          color = CRGB(r, g, b);
          sendColor(color);
        }
    }
    else if (String(topic) == topicModeSet)
    {
      mode = message.toInt();
      modeChanged = true;
      sendMode(mode);
    }
  }

  void connectWiFi()
  {
    if (WiFi.status() == WL_CONNECTED)
      return;

    Serial.println("\nConnecting to WiFi...");
    WiFi.begin(wifiSsid, wifiPass);

    while (WiFi.status() != WL_CONNECTED)
    {
      delay(500);
      Serial.print(".");
      yield();
    }

    Serial.printf("\nWiFi connected: %s\n", WiFi.localIP().toString().c_str());
  }

  void connectMQTT()
  {
    while (!mqtt.connected())
    {
      Serial.println("Connecting to MQTT...");
      if (mqtt.connect(clientId, mqttUser, mqttPass))
      {
        Serial.println("MQTT connected!");
        mqtt.subscribe(topicCommand.c_str());
        mqtt.subscribe(topicBrightSet.c_str());
        mqtt.subscribe(topicColorSet.c_str());
        mqtt.subscribe(topicModeSet.c_str());
      }
      else
      {
        Serial.printf("MQTT failed, rc=%d - retrying in 5s\n", mqtt.state());
        delay(5000);
        yield();
      }
    }
  }
};