
/** Sender part of esp-now, sends counter to broadcast address every 5 seconds.
 * Receiver should be running to receive and print the message.
 */
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <espnow.h>

// Must match the receiver's WiFi channel
const uint8_t CHANNEL = 9;
const uint32_t TX_TIMEOUT = 5; // seconds between sends

// Receiver MAC address — update to match your supermini
uint8_t RECEIVER_ADDR[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// Must match PEER_KEY on the receiver (16 bytes)
const uint8_t PEER_KEY[16] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                              0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10};

typedef struct Message
{
  char text[32];
  int value;
} Message;

static int counter = 0;

void onSent(uint8_t *mac, uint8_t status)
{
  Serial.printf("Send status: %s\n", status == 0 ? "OK" : "FAIL");
}

void setup()
{
  Serial.begin(74880);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  wifi_set_channel(CHANNEL);

  Serial.printf("Sender MAC: %s\n", WiFi.macAddress().c_str());

  if (esp_now_init() != 0)
  {
    Serial.println("ESP-NOW init failed");
    return;
  }

  esp_now_set_self_role(ESP_NOW_ROLE_CONTROLLER);
  esp_now_register_send_cb(onSent);

  esp_now_add_peer(RECEIVER_ADDR, ESP_NOW_ROLE_SLAVE, CHANNEL, (uint8_t *)PEER_KEY, 16);

  Serial.printf("ESP-NOW sender ready, TX every %ds on channel %d\n", TX_TIMEOUT, CHANNEL);
}

void loop()
{
  Message msg;
  snprintf(msg.text, sizeof(msg.text), "hello");
  msg.value = counter++;

  Serial.printf("Sending value: %d\n", msg.value);
  esp_now_send(RECEIVER_ADDR, (uint8_t *)&msg, sizeof(msg));

  delay(TX_TIMEOUT * 1000);
}