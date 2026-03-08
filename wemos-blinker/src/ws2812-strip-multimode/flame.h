#include <FastLED.h>
#include "etc/noise2d.h"

class Flame
{
public:
  Flame(CRGB *leds, int ledCount) : leds(leds), ledCount(ledCount), perlin() {};

  void show(uint32_t shift)
  {
    for (int i = 0; i < ledCount; i++)
    {
      // x - позиция LED, t - анимация во времени
      float val = perlin.noise01(i * 0.1, shift * 0.1 * speed);

      uint8_t brightness = val * 255; // 0..255
      leds[i] = CHSV(hue + (val * 40 - shift * hueRotateSpeed - 10), 255, brightness);
    }
  }

private:
  int hue = 0;            // red, obviously
  int hueRotateSpeed = 0; // 0 to make it look like flame
  int speed = 1;
  CRGB *leds;
  int ledCount;
  noise2d::Perlin2D perlin;
};