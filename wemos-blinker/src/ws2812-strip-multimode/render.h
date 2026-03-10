#include "modes/flame/flame.h"
#include "modes/rainbow/rainbow.h"

enum Modes
{
  MODE_COLOR = 0,
  MODE_RAINBOW,
  MODE_FLAME,
  MODE_RAINBOW_FIRE,
  MODE_FINE_FLAME,
  MODES_COUNT, // bootstrap to get mode amount
};

class Renderer
{
public:
  Renderer(CRGB *leds, int ledCount) : leds(leds), ledCount(ledCount), rainbow(leds, ledCount), flame(leds, ledCount) {};

  void render(uint32_t step, Modes mode, CRGB color)
  {
    switch (mode)
    {
    case MODE_COLOR:
      fill_solid(leds, ledCount, color);
      break;
    case MODE_RAINBOW:
      rainbow.show(step);
      break;
    case MODE_FLAME:
      flame.show(step, PRESET_FIRE);
      break;
    case MODE_RAINBOW_FIRE:
      flame.show(step, PRESET_RAINBOW_FIRE);
      break;
    case MODE_FINE_FLAME:
      flame.show(step, PRESET_FINE_FLAME);
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