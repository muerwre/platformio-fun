#pragma once

enum FlamePreset
{
  PRESET_FIRE = 0,
  PRESET_RAINBOW_FIRE,
  PRESET_FAST_FIRE,
  PRESET_SLOW_FIRE,
  PRESET_COUNT
};

struct FlameConfig
{
  int hueRotateSpeed;
  float speed;
  float variation;
  float hueDeviation;
  const char *name;
};

static const FlameConfig FLAME_PRESETS[PRESET_COUNT] = {
    {0, 1.0, 0.1, 0.15, "Fire"},        // PRESET_FIRE
    {1, 1.0, 0.1, 0.0, "Rainbow Fire"}, // PRESET_RAINBOW_FIRE
    {0, 2.0, 0.1, 0.15, "Fast Fire"},   // PRESET_FAST_FIRE
    {0, 0.3, 0.1, 0.15, "Slow Fire"},   // PRESET_SLOW_FIRE
};
