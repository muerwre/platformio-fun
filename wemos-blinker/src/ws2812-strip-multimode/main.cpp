#include <Arduino.h>
#include <FastLED.h>
#include "secrets.h"
#include "render.h"
#include "remote.h"

#define LED_PIN D3
#define LED_COUNT 55 // I've burned 5 leds while testing this, lol
#define COLOR_ORDER GRB
#define CHIPSET WS2812

#define BRIGHTNESS 255             // limited by FastLED.setMaxPowerInVoltsAndMilliamps anyway
#define MODE_SWITCH_INTERVAL 10000 // switch modes every 10 seconds

CRGB leds[LED_COUNT];

Renderer renderer(leds, LED_COUNT);
RemoteControl remote(WIFI_SSID, WIFI_PASS, MQTT_SERVER, MQTT_PORT, MQTT_USER, MQTT_PASS, "esp8266-strip", MQTT_TOPIC);

Modes parseMode(int value)
{
  if (value >= 0 && value < MODES_COUNT)
    return (Modes)value;
  return MODE_RAINBOW; // fallback
}

Modes currentMode = MODE_RAINBOW;

void setup()
{
  Serial.begin(115200);
  FastLED.addLeds<CHIPSET, LED_PIN, COLOR_ORDER>(leds, LED_COUNT).setCorrection(TypicalSMD5050);
  FastLED.setBrightness(BRIGHTNESS);
  FastLED.setMaxPowerInVoltsAndMilliamps(5, 1000);
  FastLED.clear(true);
  remote.setup();
}

void loop()
{
  remote.loop();

  if (remote.isModeChanged())
  {
    currentMode = parseMode(remote.getMode());
    Serial.printf("Mode changed to: %d\n", currentMode);
    remote.clearModeChanged();
  }

  if (remote.isBrightnessChanged())
  {
    FastLED.setBrightness(remote.getBrightness());
    remote.clearBrightnessChanged();
  }

  if (remote.isStatusChanged())
  {
    Serial.printf("Status changed to: %s\n", remote.getStatus() ? "ON" : "OFF");
    remote.clearStatusChanged();
  }

  if (!remote.getStatus())
  {
    FastLED.clear(true);

    delay(10000); // a.k.a. sleep :-)

    return;
  }

  renderer.render(millis() / LED_COUNT, currentMode, remote.getColor());

  FastLED.show();
}
