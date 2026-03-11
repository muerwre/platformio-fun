#pragma once
#include <FastLED.h>
#include "noise2d.h"
#include "presets.h"

class Flame
{
public:
  Flame(CRGB *leds, int ledCount) : leds(leds), ledCount(ledCount), perlin() {};

  void show(uint32_t shift, uint8_t hue, FlamePreset preset = PRESET_FIRE)
  {
    const FlameConfig &config = FLAME_PRESETS[preset];

    for (int i = 0; i < ledCount; i++)
    {
      float val = perlin.noise01(i * config.variation, shift * 0.1 * config.speed);

      uint8_t brightness = val * 255;
      leds[i] = CHSV(hue + (val * 255 * config.hueDeviation - shift * config.hueRotateSpeed - 10), 255, brightness);
    }
  }

private:
  CRGB *leds;
  int ledCount;
  noise2d::Perlin2D perlin;
};
