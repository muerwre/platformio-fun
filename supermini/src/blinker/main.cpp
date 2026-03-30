#include <Adafruit_NeoPixel.h>

#define PIN 8       // Onboard RGB LED pin
#define NUMPIXELS 1 // Number of LEDs

Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

void setup()
{
  pixels.begin();
}

void loop()
{
  pixels.setPixelColor(0, pixels.Color(255, 0, 0)); // Red
  pixels.show();
  delay(500);
  pixels.setPixelColor(0, pixels.Color(0, 255, 0)); // Green
  pixels.show();
  delay(500);
  pixels.setPixelColor(0, pixels.Color(0, 0, 255)); // Blue
  pixels.show();
  delay(500);
  pixels.show();
}