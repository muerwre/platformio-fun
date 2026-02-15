// Example: https://wokwi.com/projects/289186888566178317
// FontGen: https://pjrp.github.io/MDParolaFontEditor
// Examples: https://github.com/MajicDesigns/MD_Parola/tree/main/examples
// Lib Docs: https://majicdesigns.github.io/MD_MAX72XX/page_f_c16.html
#include <MD_MAXPanel.h>

#define USE_LOCAL_FONT 1
#include "font.h"
// #include "Font5x3.h"

// LEDs
#define USE_GENERIC_HW 1
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES 3 // Define the number of displays connected
#define CLK_PIN 12    // CLK or SCK
#define DATA_PIN 11   // DIN or DATA or MOSI
#define CS_PIN 10     // CS or SS
#define MSG_LEN 64
#define SCROLL_INTERVAL 30 // ms between moves (lower = faster)

int scrollX = 0;
int textWidth = 0;
unsigned long lastScroll = 0;

MD_MAXPanel mp = MD_MAXPanel(HARDWARE_TYPE, CS_PIN, 3, 1);
char msg[MSG_LEN] = "<---";

void setup()
{
  Serial.begin(9600);
  mp.begin();
  textWidth = mp.getTextWidth(msg);
}

void loop()
{
  // --- Read Serial (non-blocking) ---
  if (Serial.available() > 0)
  {
    int len = Serial.readBytesUntil('\n', msg, MSG_LEN - 1);
    msg[len] = '\0';

    // reset scroll for new message
    textWidth = mp.getTextWidth(msg);
    scrollX = -8 * MAX_DEVICES;
  }

  // --- Scroll timing ---
  unsigned long now = millis();
  if (now - lastScroll >= SCROLL_INTERVAL)
  {
    lastScroll = now;

    mp.clear();
    mp.drawText(-scrollX, mp.getFontHeight() - 2, msg);

    scrollX++;

    // restart when text is fully off screen
    if (scrollX > textWidth + 4)
    {
      scrollX = -8 * MAX_DEVICES;
    }
  }
}