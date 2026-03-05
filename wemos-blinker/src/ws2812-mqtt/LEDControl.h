#ifndef LED_CONTROL_H
#define LED_CONTROL_H

#include <FastLED.h>

template <uint8_t PIN, uint8_t WIDTH, uint8_t HEIGHT, EOrder COLOR_ORDER = GRB>
class LEDControl
{
private:
  static constexpr uint16_t NUM_LEDS = WIDTH * HEIGHT;
  CRGB leds[NUM_LEDS];

  bool isOn = false;
  uint8_t currentBrightness = 16;
  CRGB currentColor = CRGB::Black;

  const bool kMatrixSerpentineLayout = true;
  const bool kMatrixVertical = false;

  uint16_t XY(uint8_t x, uint8_t y)
  {
    uint16_t i;

    if (kMatrixSerpentineLayout == false)
    {
      if (kMatrixVertical == false)
      {
        i = (y * WIDTH) + x;
      }
      else
      {
        i = HEIGHT * (WIDTH - (x + 1)) + y;
      }
    }

    if (kMatrixSerpentineLayout == true)
    {
      if (kMatrixVertical == false)
      {
        if (y & 0x01)
        {
          // Odd rows run backwards
          uint8_t reverseX = (WIDTH - 1) - x;
          i = (y * WIDTH) + reverseX;
        }
        else
        {
          // Even rows run forwards
          i = (y * WIDTH) + x;
        }
      }
      else
      { // vertical positioning
        if (x & 0x01)
        {
          i = HEIGHT * (WIDTH - (x + 1)) + y;
        }
        else
        {
          i = HEIGHT * (WIDTH - x) - (y + 1);
        }
      }
    }

    return i;
  }

  void updateLEDs()
  {
    if (isOn)
    {
      Serial.printf("Updating %d LEDs to RGB: %d,%d,%d\n", NUM_LEDS, currentColor.r, currentColor.g, currentColor.b);

      for (uint16_t i = 0; i < NUM_LEDS; i++)
      {
        leds[i] = currentColor;
      }
    }
    else
    {
      Serial.println("Turning off LEDs");
      fill_solid(leds, NUM_LEDS, CRGB::Black);
    }

    // Disable interrupts during FastLED.show() to prevent WiFi interference
    noInterrupts();
    FastLED.show();
    interrupts();
  }

public:
  LEDControl()
  {
  }

  template <template <uint8_t, EOrder> class CHIPSET>
  void begin()
  {
    Serial.printf("Initializing LEDs: NUM_LEDS=%d, WIDTH=%d, HEIGHT=%d\n", NUM_LEDS, WIDTH, HEIGHT);
    FastLED.addLeds<CHIPSET, PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalSMD5050);
    FastLED.setBrightness(currentBrightness);
    FastLED.clear();
    FastLED.show();
  }

  void setRGB(uint8_t r, uint8_t g, uint8_t b)
  {
    currentColor = CRGB(r, g, b);
  }

  void turnOn()
  {
    isOn = true;
  }

  void turnOff()
  {
    isOn = false;
  }

  void setBrightness(uint8_t brightness)
  {
    currentBrightness = brightness;
    FastLED.setBrightness(currentBrightness);
  }

  void refresh()
  {
    updateLEDs();
  }

  bool getState() const
  {
    return isOn;
  }

  uint8_t getBrightness() const
  {
    return currentBrightness;
  }

  CRGB getColor() const
  {
    return currentColor;
  }

  uint8_t getWidth() const
  {
    return WIDTH;
  }

  uint8_t getHeight() const
  {
    return HEIGHT;
  }
};

#endif
