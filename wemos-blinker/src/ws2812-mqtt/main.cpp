#include <FastLED.h>
#include "LEDControl.h"

#define LED_PIN D3
#define MATRIX_WIDTH 16
#define MATRIX_HEIGHT 16

LEDControl<LED_PIN, MATRIX_WIDTH, MATRIX_HEIGHT> ledControl;

void setup()
{
  ledControl.begin<WS2812>();
  ledControl.turnOn();
}

void loop()
{
  ledControl.setRGB(0, 0, 255); // Set blue color
  delay(1000);
  ledControl.setRGB(255, 0, 0); // Set red color
  delay(1000);
  ledControl.setRGB(0, 255, 0); // Set green color
  delay(1000);
}