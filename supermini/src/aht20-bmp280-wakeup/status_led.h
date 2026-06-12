#pragma once
#include <Adafruit_NeoPixel.h>

// Onboard WS2812 RGB LED (GPIO8) used as a simple solid-colour status light.
class StatusLed
{
public:
  StatusLed(uint8_t pin, uint8_t brightness = 10)
      : led(1, pin, NEO_GRB + NEO_KHZ800), brightness(brightness) {}

  void begin()
  {
    led.begin();
    led.setBrightness(brightness);
    off();
  }

  void blue() { show(led.Color(0, 0, 255)); } // taking a reading
  void red() { show(led.Color(255, 0, 0)); }   // woken by motion
  void off() { show(0); }

private:
  Adafruit_NeoPixel led;
  uint8_t brightness;

  void show(uint32_t color)
  {
    led.setPixelColor(0, color);
    led.show();
  }
};
