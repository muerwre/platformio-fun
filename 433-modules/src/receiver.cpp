/**
 * 433 receiver module for Arduino Uno, done by tutorial from
 * https://www.youtube.com/watch?v=b5C9SPVlU4U&t=235s
 *
 * WIRING:
 * VCC -> 5V
 * GND -> GND
 * DATA -> D11
 *
 */

#include <Arduino.h>
#include <RH_ASK.h>
#include <SPI.h> // Not actualy used but needed to compile

RH_ASK rf_driver;

static uint32_t nonce = 0;
static uint32_t packets_received = 0;
static uint32_t packets_lost = 0;

void setup()
{
  rf_driver.init();
  Serial.begin(9600);
  Serial.println("available");
}

void loop()
{
  uint8_t buf[sizeof(nonce)];
  uint8_t buflen = sizeof(buf);

  if (rf_driver.recv(buf, &buflen))
  {
    uint32_t inval = *(uint32_t *)buf;
    packets_received++;

    if (inval < nonce)
    {
      Serial.println("Out of order packet received, starting again");
      nonce = inval + 1;

      return;
    }

    if (nonce != 0)
    {
      packets_lost += inval - nonce;
    }
    else
    {
      Serial.println("First packet received");
    }

    nonce = inval + 1;

    Serial.print("received: ");
    Serial.print(packets_received);
    Serial.print("\tlost: ");
    Serial.print(packets_lost);
    Serial.print("\t\t\t\tlast packet: ");
    Serial.print(inval);
    Serial.println();
  }
}
