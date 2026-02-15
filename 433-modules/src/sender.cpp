/**
 * 433 receiver module for Arduino Uno, done by tutorial from
 * https://www.youtube.com/watch?v=b5C9SPVlU4U&t=235s
 *
 * WIRING:
 * VCC -> 5V
 * GND -> GND
 * DATA -> D12 (there're 2 pins, connect any of them, they're soldered together anyway)
 *
 */

#include <Arduino.h>
#include <RH_ASK.h>
#include <SPI.h> // Not actualy used but needed to compile

RH_ASK rf_driver;

uint32_t nonce = 0;

void setup()
{
  rf_driver.init();
  pinMode(LED_BUILTIN, OUTPUT);
}

void loop()
{
  digitalWrite(LED_BUILTIN, HIGH);
  rf_driver.send((uint8_t *)&nonce, sizeof(nonce));
  rf_driver.waitPacketSent();
  digitalWrite(LED_BUILTIN, LOW);
  nonce++;
  delay(500);
}