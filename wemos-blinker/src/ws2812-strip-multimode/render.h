#include "modes/flame/flame.h"
#include "modes/rainbow/rainbow.h"

enum Modes
{
  MODE_RAINBOW = 0,
  MODE_FLAME,
  MODE_RAINBOW_FIRE,
  MODES_COUNT, // bootstrap to get mode amount
};

class Renderer
{
public:
  Renderer(CRGB *leds, int ledCount) : rainbow(leds, ledCount), flame(leds, ledCount) {};

  void render(uint32_t step, Modes mode)
  {
    switch (mode)
    {
    case MODE_RAINBOW:
      rainbow.show(step);
      break;
    case MODE_FLAME:
      flame.show(step, PRESET_FIRE);
      break;
    case MODE_RAINBOW_FIRE:
      flame.show(step, PRESET_RAINBOW_FIRE);
      break;
    default:
      break;
    }
  }

private:
  Rainbow rainbow;
  Flame flame;
};