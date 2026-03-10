#include <Arduino.h>
#include <FastLED.h>
#include "rainbow.h"
#include "flame/main.h"
#define LED_PIN D3
#define LED_COUNT 60
#define COLOR_ORDER GRB
#define CHIPSET WS2812B

#define BRIGHTNESS 64
// delay between frames (ms)
#define FRAME_DELAY 1

CRGB leds[LED_COUNT];

Rainbow rainbow(leds, LED_COUNT);
Flame flame(leds, LED_COUNT);

void setup()
{
  Serial.begin(115200);
  FastLED.addLeds<CHIPSET, LED_PIN, COLOR_ORDER>(leds, LED_COUNT).setCorrection(TypicalSMD5050);
  FastLED.setBrightness(BRIGHTNESS);

  FastLED.setMaxPowerInVoltsAndMilliamps(5, 500);
}

void loop()
{
  uint32_t step = millis() / FRAME_DELAY / LED_COUNT;

  // rainbow.show(step);
  flame.show(step);
  FastLED.show();
}