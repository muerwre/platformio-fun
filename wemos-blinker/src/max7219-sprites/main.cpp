// FROM https://github.com/MajicDesigns/MD_MAX72XX/tree/main/examples/MD_MAX72xx_Pacman

#include <MD_MAX72xx.h>
#include <SPI.h>

// --------------------
// MD_MAX72xx hardware definitions and object
// Define the number of devices we have in the chain and the hardware interface
// NOTE: These pin numbers will probably not work with your hardware and may
// need to be adapted
//
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES 3
#define CLK_PIN D5
#define DATA_PIN D7
#define CS_PIN D4
// VCC to 3.3v

MD_MAX72XX mx = MD_MAX72XX(HARDWARE_TYPE, CS_PIN, MAX_DEVICES); // SPI hardware interface
// MD_MAX72XX mx = MD_MAX72XX(HARDWARE_TYPE, DATA_PIN, CLK_PIN, CS_PIN, MAX_DEVICES); // Arbitrary pins

// --------------------
// Constant parameters
//
#define ANIMATION_DELAY 75 // milliseconds
#define MAX_FRAMES 4       // number of animation frames

// ========== General Variables ===========
//
const uint8_t pacman[MAX_FRAMES][18] = // ghost pursued by a pacman
    {
        {0xfe, 0x73, 0xfb, 0x7f, 0xf3, 0x7b, 0xfe, 0x00, 0x00, 0x00, 0x3c, 0x7e, 0x7e, 0xff, 0xe7, 0xc3, 0x81, 0x00},
        {0xfe, 0x7b, 0xf3, 0x7f, 0xfb, 0x73, 0xfe, 0x00, 0x00, 0x00, 0x3c, 0x7e, 0xff, 0xff, 0xe7, 0xe7, 0x42, 0x00},
        {0xfe, 0x73, 0xfb, 0x7f, 0xf3, 0x7b, 0xfe, 0x00, 0x00, 0x00, 0x3c, 0x7e, 0xff, 0xff, 0xff, 0xe7, 0x66, 0x24},
        {0xfe, 0x7b, 0xf3, 0x7f, 0xf3, 0x7b, 0xfe, 0x00, 0x00, 0x00, 0x3c, 0x7e, 0xff, 0xff, 0xff, 0xff, 0x7e, 0x3c},
};
const uint8_t DATA_WIDTH = (sizeof(pacman[0]) / sizeof(pacman[0][0]));

uint32_t prevTimeAnim = 0; // remember the millis() value in animations
int16_t idx;               // display index (column)
uint8_t frame;             // current animation frame
uint8_t deltaFrame;        // the animation frame offset for the next frame

// ========== Control routines ===========
//
void resetMatrix(void)
{
  mx.control(MD_MAX72XX::INTENSITY, MAX_INTENSITY / 2);
  mx.control(MD_MAX72XX::UPDATE, MD_MAX72XX::ON);
  mx.clear();
}

void setup()
{
  mx.begin();
  resetMatrix();
  prevTimeAnim = millis();
}

void loop(void)
{
  static boolean bInit = true; // initialise the animation

  // Is it time to animate?
  if (millis() - prevTimeAnim < ANIMATION_DELAY)
    return;
  prevTimeAnim = millis(); // starting point for next time

  mx.control(MD_MAX72XX::UPDATE, MD_MAX72XX::OFF);

  // Initialize
  if (bInit)
  {
    mx.clear();
    idx = -DATA_WIDTH;
    frame = 0;
    deltaFrame = 1;
    bInit = false;
  }

  // clear old graphic
  for (uint8_t i = 0; i < DATA_WIDTH; i++)
    mx.setColumn(idx - DATA_WIDTH + i, 0);
  // move reference column and draw new graphic
  idx++;
  for (uint8_t i = 0; i < DATA_WIDTH; i++)
    mx.setColumn(idx - DATA_WIDTH + i, pacman[frame][i]);

  // advance the animation frame
  frame += deltaFrame;
  if (frame == 0 || frame == MAX_FRAMES - 1)
    deltaFrame = -deltaFrame;

  // check if we are completed and set initialise for next time around
  bInit = (idx == mx.getColumnCount() + DATA_WIDTH);

  mx.control(MD_MAX72XX::UPDATE, MD_MAX72XX::ON);

  return;
}