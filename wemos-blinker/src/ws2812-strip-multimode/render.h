#include "modes/flame/flame.h"
#include "modes/rainbow/rainbow.h"

enum Modes
{
  MODE_COLOR = 0,
  MODE_RAINBOW,
  MODE_FLAME,
  MODE_RAINBOW_FIRE,
  MODE_FAST_FIRE,
  MODE_SLOW_FIRE,
  MODES_COUNT,
};

class Renderer
{
public:
  Renderer(CRGB *leds, int ledCount) : leds(leds), ledCount(ledCount), rainbow(leds, ledCount), flame(leds, ledCount) {};

  void render(uint32_t step, Modes mode, CRGB color)
  {
    CHSV hsv = rgb2hsv_approximate(color);

    switch (mode)
    {
    case MODE_COLOR:
      fill_solid(leds, ledCount, color);
      break;
    case MODE_RAINBOW:
      rainbow.show(step);
      break;
    case MODE_FLAME:
      flame.show(step, hsv.hue, PRESET_FIRE);
      break;
    case MODE_RAINBOW_FIRE:
      flame.show(step, hsv.hue, PRESET_RAINBOW_FIRE);
      break;
    case MODE_FAST_FIRE:
      flame.show(step, hsv.hue, PRESET_FAST_FIRE);
      break;
    case MODE_SLOW_FIRE:
      flame.show(step, hsv.hue, PRESET_SLOW_FIRE);
      break;
    default:
      break;
    }
  }

private:
  CRGB *leds;
  int ledCount;
  Rainbow rainbow;
  Flame flame;
};