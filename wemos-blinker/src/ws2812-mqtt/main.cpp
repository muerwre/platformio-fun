#include <FastLED.h>

#define LED_PIN D3

#define COLOR_ORDER GRB
#define CHIPSET WS2812

#define BRIGHTNESS 16

// Params for width and height
const uint8_t kMatrixWidth = 16;
const uint8_t kMatrixHeight = 16;

// Param for different pixel layouts
const bool kMatrixSerpentineLayout = true;
const bool kMatrixVertical = false;

uint16_t XY(uint8_t x, uint8_t y)
{
  uint16_t i;

  if (kMatrixSerpentineLayout == false)
  {
    if (kMatrixVertical == false)
    {
      i = (y * kMatrixWidth) + x;
    }
    else
    {
      i = kMatrixHeight * (kMatrixWidth - (x + 1)) + y;
    }
  }

  if (kMatrixSerpentineLayout == true)
  {
    if (kMatrixVertical == false)
    {
      if (y & 0x01)
      {
        // Odd rows run backwards
        uint8_t reverseX = (kMatrixWidth - 1) - x;
        i = (y * kMatrixWidth) + reverseX;
      }
      else
      {
        // Even rows run forwards
        i = (y * kMatrixWidth) + x;
      }
    }
    else
    { // vertical positioning
      if (x & 0x01)
      {
        i = kMatrixHeight * (kMatrixWidth - (x + 1)) + y;
      }
      else
      {
        i = kMatrixHeight * (kMatrixWidth - x) - (y + 1);
      }
    }
  }

  return i;
}

#define NUM_LEDS (kMatrixWidth * kMatrixHeight)
CRGB leds_plus_safety_pixel[NUM_LEDS + 1];
CRGB *const leds(leds_plus_safety_pixel + 1);

uint16_t XYsafe(uint8_t x, uint8_t y)
{
  if (x >= kMatrixWidth)
    return -1;
  if (y >= kMatrixHeight)
    return -1;
  return XY(x, y);
}

void DrawOneFrame(uint8_t startHue8, int8_t yHueDelta8, int8_t xHueDelta8)
{
  uint8_t lineStartHue = startHue8;
  for (uint8_t y = 0; y < kMatrixHeight; y++)
  {
    lineStartHue += yHueDelta8;
    uint8_t pixelHue = lineStartHue;
    for (uint8_t x = 0; x < kMatrixWidth; x++)
    {
      pixelHue += xHueDelta8;
      leds[XY(x, y)] = CHSV(pixelHue, 255, 255);
    }
  }
}

void setup()
{
  FastLED.addLeds<CHIPSET, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalSMD5050);
  FastLED.setBrightness(BRIGHTNESS);
}

// Demo that USES "XY" follows code below

void loop()
{
  uint32_t ms = millis();
  int32_t yHueDelta32 = ((int32_t)cos16(ms * (27 / 1)) * (350 / kMatrixWidth));
  int32_t xHueDelta32 = ((int32_t)cos16(ms * (39 / 1)) * (310 / kMatrixHeight));
  DrawOneFrame(ms / 65536, yHueDelta32 / 32768, xHueDelta32 / 32768);
  if (ms < 5000)
  {
    FastLED.setBrightness(scale8(BRIGHTNESS, (ms * 256) / 5000));
  }
  else
  {
    FastLED.setBrightness(BRIGHTNESS);
  }
  FastLED.show();
}