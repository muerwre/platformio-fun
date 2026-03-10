#include <Arduino.h>
#include <FastLED.h>

class Rainbow
{
public:
  Rainbow(CRGB *leds, int ledCount) : leds(leds), ledCount(ledCount) {};

  void show(uint32_t step)
  {
    // Fill LEDs with a rainbow that shifts each frame
    for (int i = 0; i < ledCount; ++i)
    {
      // Spread the hue across the strip, add startHue to shift
      uint8_t hue = (step * speed) + (uint8_t)((256UL * i) / ledCount);
      leds[i] = CHSV(hue, 255, 255);
    }
  }

private:
  CRGB *leds;
  int ledCount;
  uint8_t speed = 5;
};
