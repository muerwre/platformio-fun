#include <Arduino.h>
#include "secrets.h"
#include "status_led.h"
#include "mesh_pairing.h"
#include "mesh_node.h"

// Onboard WS2812 RGB LED of the ESP32-C6 Supermini.
#define LED_PIN 8

// How long to keep the red error LED on before re-attempting pairing.
#define RETRY_DELAY_MS 5000

StatusLed led(LED_PIN);
MeshPairing pairing;
MeshNode node;

uint32_t errorAt = 0;
bool nodeStarted = false;
bool helloSent = false;

void setup()
{
  Serial.begin(115200);
  delay(200);
  Serial.println("\n=== Meshtastic connector: pairing ===");

  led.begin();
  pairing.begin(MESH_BLE_MAC, MESH_BLE_PIN);
  pairing.startSearching();
}

void loop()
{
  led.update();

  switch (pairing.update())
  {
  case MeshPairing::SEARCHING:
    led.setMode(StatusLed::SEARCHING); // blue blinking
    break;

  case MeshPairing::PAIRED:
    led.setMode(StatusLed::PAIRED); // green blinking

    // Once bonded, open the protocol and ask the node for its info.
    if (!nodeStarted)
    {
      nodeStarted = node.begin(pairing.getClient());
      if (nodeStarted)
        node.requestConfig();
    }
    else
    {
      node.poll();

      // Once we've read the node DB, say hello (once).
      // Broadcast -> post to MESH_CHANNEL's feed; otherwise deliver straight to the node.
      if (node.isComplete() && !helloSent)
      {
        uint8_t channel = (MESH_DEST_ID == MeshNode::BROADCAST_ADDR) ? MESH_CHANNEL : 0;
        node.sendText(MESH_DEST_ID, "Hello", channel);
        helloSent = true;
      }
    }
    break;

  case MeshPairing::FAILED:
    led.setMode(StatusLed::PAIR_ERROR); // solid red
    if (errorAt == 0)
    {
      errorAt = millis();
      Serial.printf("Pairing failed, retrying in %d ms\n", RETRY_DELAY_MS);
    }
    else if (millis() - errorAt >= RETRY_DELAY_MS)
    {
      errorAt = 0;
      nodeStarted = false;
      helloSent = false;
      pairing.startSearching();
    }
    break;
  }

  delay(10);
}
