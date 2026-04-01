/**
 * Demonstrates esp-now receiver WITH simultaneous connection to WIFI
 * and reporting to MQTT.
 *
 * Sender should know MAC address (or defined it as bunch of FFs for broadcast)
 * and channel (should match WiFi channel on sender).
 */

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <PubSubClient.h>
#include "secrets.h"

typedef struct Message
{
  char text[32];
  int value;
} Message;

// Must match PEER_KEY on the sender (16 bytes)
const uint8_t PEER_KEY[16] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                              0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10};

WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);

// Pending publish buffer — filled in the ESP-NOW callback, drained in loop()
volatile bool pendingPublish = false;
char pendingPayload[64];

volatile unsigned long ledOffAt = 0;

void mqttReconnect()
{
  while (!mqtt.connected())
  {
    Serial.print("Connecting to MQTT...");
    if (mqtt.connect("supermini-espnow", MQTT_USER, MQTT_PASS))
    {
      Serial.println(" connected");
    }
    else
    {
      Serial.printf(" failed (rc=%d), retrying in 5s\n", mqtt.state());
      delay(5000);
    }
  }
}

void onDataReceived(const esp_now_recv_info_t *info, const uint8_t *data, int len)
{
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
           info->src_addr[0], info->src_addr[1], info->src_addr[2],
           info->src_addr[3], info->src_addr[4], info->src_addr[5]);

  Serial.printf("Received %d bytes from %s\n", len, macStr);

  if (len == sizeof(Message))
  {
    Message msg;
    memcpy(&msg, data, sizeof(msg));
    Serial.printf("  text: %s, value: %d\n", msg.text, msg.value);
    snprintf(pendingPayload, sizeof(pendingPayload), "{\"text\":\"%s\",\"value\":%d}", msg.text, msg.value);
    pendingPublish = true;
    ledOffAt = millis() + 500;
    digitalWrite(LED_BUILTIN, HIGH);
  }
}

void setup()
{
  Serial.begin(115200);

  // Connect to WiFi first so ESP-NOW inherits the channel
  WiFi.mode(WIFI_STA);
  Serial.printf("Connecting to %s", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.printf("\nConnected, IP: %s, channel: %d\n",
                WiFi.localIP().toString().c_str(), WiFi.channel());
  Serial.printf("Receiver MAC: %s\n", WiFi.macAddress().c_str());

  // Force the channel so senders can be configured to match
  esp_wifi_set_channel(WiFi.channel(), WIFI_SECOND_CHAN_NONE);

  mqtt.setServer(MQTT_SERVER, MQTT_PORT);

  if (esp_now_init() != ESP_OK)
  {
    Serial.println("ESP-NOW init failed");
    return;
  }

  // esp_now_set_pmk(PEER_KEY);

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  esp_now_register_recv_cb(onDataReceived);
  Serial.printf("ESP-NOW ready on channel %d\n", WiFi.channel());
}

void loop()
{
  if (!mqtt.connected())
    mqttReconnect();
  mqtt.loop();

  if (pendingPublish)
  {
    pendingPublish = false;
    mqtt.publish(MQTT_TOPIC, pendingPayload);
    Serial.printf("Published to %s: %s\n", MQTT_TOPIC, pendingPayload);
  }

  if (ledOffAt && millis() >= ledOffAt)
  {
    ledOffAt = 0;
    digitalWrite(LED_BUILTIN, LOW);
  }
}
