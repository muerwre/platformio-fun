#pragma once

enum FlamePreset
{
  PRESET_FIRE = 0,
  PRESET_BLUE_FLAME,
  PRESET_GREEN_FLAME,
  PRESET_RAINBOW_FIRE,
  PRESET_SLOW_EMBER,
  PRESET_FAST_ORANGE,
  PRESET_PURPLE_FIRE,
  PRESET_FINE_FLAME,
  PRESET_COUNT
};

struct FlameConfig
{
  int hue;
  int hueRotateSpeed;
  float speed;
  float variation;
  const char *name;
};

static const FlameConfig FLAME_PRESETS[PRESET_COUNT] = {
    {0, 0, 1.0, 0.1, "Fire"},          // PRESET_FIRE
    {160, 0, 1.0, 0.1, "Blue Flame"},  // PRESET_BLUE_FLAME
    {96, 0, 1.0, 0.1, "Green Flame"},  // PRESET_GREEN_FLAME
    {0, 1, 1.0, 0.1, "Rainbow Fire"},  // PRESET_RAINBOW_FIRE
    {0, 0, 0.3, 0.1, "Slow Ember"},    // PRESET_SLOW_EMBER
    {20, 0, 2.0, 0.1, "Fast Orange"},  // PRESET_FAST_ORANGE
    {200, 0, 1.0, 0.1, "Purple Fire"}, // PRESET_PURPLE_FIRE
    {0, 0, 1.0, 0.05, "Fine Flame"},   // PRESET_FINE_FLAME
};
