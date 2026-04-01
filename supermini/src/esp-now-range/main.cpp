#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <Adafruit_NeoPixel.h>

const uint8_t CHANNEL = 9;

#define LED_PIN 8
Adafruit_NeoPixel led(1, LED_PIN, NEO_GRB + NEO_KHZ800);

typedef struct Message
{
  char text[32];
  int value;
} Message;

// Must match PEER_KEY on the sender (16 bytes)
const uint8_t PEER_KEY[16] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                              0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10};

unsigned long lastReceivedAt = 0;
unsigned long ledOffAt = 0;
uint32_t flashColor = 0;

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
  }

  unsigned long now = millis();

  // color based on gap since previous packet
  if (lastReceivedAt == 0)
    flashColor = led.Color(0, 255, 0); // first packet — green
  else
  {
    unsigned long gap = now - lastReceivedAt;
    if (gap < 2000)
      flashColor = led.Color(0, 255, 0); // green
    else if (gap < 5000)
      flashColor = led.Color(255, 165, 0); // yellow
    else
      flashColor = led.Color(255, 0, 0); // red
  }

  lastReceivedAt = now;
  ledOffAt = now + 200;
}

void setup()
{
  Serial.begin(115200);

  led.begin();
  led.setBrightness(10);
  led.setPixelColor(0, led.Color(0, 0, 0));
  led.show();

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  esp_wifi_set_channel(CHANNEL, WIFI_SECOND_CHAN_NONE);

  Serial.printf("MAC: %s\n", WiFi.macAddress().c_str());

  if (esp_now_init() != ESP_OK)
  {
    Serial.println("ESP-NOW init failed");
    return;
  }

  // esp_now_set_pmk(PEER_KEY);
  esp_now_register_recv_cb(onDataReceived);
  Serial.printf("ESP-NOW range test ready on channel %d\n", CHANNEL);
}

void loop()
{
  unsigned long now = millis();

  if (ledOffAt && now < ledOffAt)
    led.setPixelColor(0, flashColor);
  else
    led.setPixelColor(0, led.Color(0, 0, 0));

  led.show();
  delay(10);
}
