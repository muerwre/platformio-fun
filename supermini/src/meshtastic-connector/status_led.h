#pragma once
#include <Adafruit_NeoPixel.h>

// Onboard WS2812 RGB LED on the ESP32-C6 Supermini.
//
// Visual states:
//   SEARCHING  -> blue, blinking
//   PAIRED     -> green, blinking
//   PAIR_ERROR -> red, solid
//
// Non-blocking: drive it by calling update() often from loop().
class StatusLed
{
public:
  enum Mode
  {
    OFF,
    SEARCHING,
    PAIRED,
    PAIR_ERROR,
  };

  StatusLed(uint8_t pin, uint8_t brightness = 10)
      : led(1, pin, NEO_GRB + NEO_KHZ800), brightness(brightness) {}

  void begin()
  {
    led.begin();
    led.setBrightness(brightness);
    show(0);
  }

  void setMode(Mode next)
  {
    if (next == mode)
      return;
    mode = next;
    lastToggle = 0; // force an immediate refresh in update()
    on = false;
  }

  // Call frequently; renders the current mode based on millis().
  void update()
  {
    switch (mode)
    {
    case SEARCHING:
      blink(led.Color(0, 0, 255)); // blue
      break;
    case PAIRED:
      blink(led.Color(0, 255, 0)); // green
      break;
    case PAIR_ERROR:
      show(led.Color(255, 0, 0)); // solid red
      break;
    case OFF:
    default:
      show(0);
      break;
    }
  }

private:
  static constexpr uint32_t BLINK_INTERVAL_MS = 300;

  Adafruit_NeoPixel led;
  uint8_t brightness;
  Mode mode = OFF;
  uint32_t lastToggle = 0;
  bool on = false;

  void blink(uint32_t color)
  {
    uint32_t now = millis();
    if (now - lastToggle < BLINK_INTERVAL_MS)
      return;
    lastToggle = now;
    on = !on;
    show(on ? color : 0);
  }

  void show(uint32_t color)
  {
    led.setPixelColor(0, color);
    led.show();
  }
};
