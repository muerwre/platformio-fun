/**
 * Wiring: https://www.esp8266learning.com/max7219-8x8-led-matrix-example.php
 *
 * RomanCyrrilic.h comes in cp1251, so we convert it using utf8.h, but
 * you can just switch ide to that encoding.
 */
#include <MD_Parola.h>
#include <MD_MAX72xx.h>
#include <SPI.h>
#include "RomanCyrrilic.h"
#include "utf8.h"

#define HARDWARE_TYPE MD_MAX72XX::FC16_HW // Necessary to work with our displays
#define MAX_DEVICES 3

#define CLK_PIN D5
#define DATA_PIN D7
#define CS_PIN D4
// VCC to 3.3v

// Hardware SPI connection
MD_Parola P = MD_Parola(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);

void setup(void)
{
  P.begin();
  P.setFont(RomanCyrillic);
  P.setIntensity(1);
  P.setSpeed(50);
}

void loop(void)
{
  static char text[] = ("АБВГДЕЁЖЗИЙКЛМНОПРСТУФХЦЧШЩЪЫЬЭЮЯ"
                        "абвгдеёжзийклмнопрстуфхцчшщъыьэюя"
                        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                        "abcdefghijklmnopqrstuvwxyz"
                        "0123456789:;<>?@[]\\^_`");
  static bool converted = false;

  if (!converted)
  {
    utf8Ascii(text);
    converted = true;
  }

  if (P.displayAnimate())
    P.displayText(text, PA_LEFT, P.getSpeed(), 0, PA_SCROLL_LEFT, PA_SCROLL_LEFT);
}
