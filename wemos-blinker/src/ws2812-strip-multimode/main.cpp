#include <Arduino.h>
#include <FastLED.h>
#include "render.h"

#define LED_PIN D3
#define LED_COUNT 55 // I've burned 5 leds while testing this, lol
#define COLOR_ORDER GRB
#define CHIPSET WS2812

#define BRIGHTNESS 128             // limited by FastLED.setMaxPowerInVoltsAndMilliamps anyway
#define MODE_SWITCH_INTERVAL 10000 // switch modes every 10 seconds

CRGB leds[LED_COUNT];

Renderer renderer(leds, LED_COUNT);

void setup()
{
  Serial.begin(115200);
  FastLED.addLeds<CHIPSET, LED_PIN, COLOR_ORDER>(leds, LED_COUNT).setCorrection(TypicalSMD5050);
  FastLED.setBrightness(BRIGHTNESS);
  FastLED.setMaxPowerInVoltsAndMilliamps(5, 500);
}

void loop()
{
  uint32_t step = millis() / LED_COUNT;
  Modes currentMode = (Modes)((millis() / MODE_SWITCH_INTERVAL) % MODES_COUNT);

  renderer.render(step, currentMode);

  FastLED.show();
}
