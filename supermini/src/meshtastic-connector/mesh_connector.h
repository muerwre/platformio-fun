#pragma once
#include <esp_sleep.h>
#include "secrets.h"
#include "status_led.h"
#include "mesh_pairing.h"
#include "mesh_node.h"

// High-level glue: pairs with the node, opens the Meshtastic protocol, sends a
// message, then deep-sleeps until the next one. Hides the LED state machine,
// BLE pairing and protocol details so main.cpp stays about the message itself.
class MeshConnector
{
public:
  // ledPin: onboard WS2812; wakeupPin: PIR motion input (GPIO0-7 for C6 wakeup).
  MeshConnector(uint8_t ledPin, uint8_t wakeupPin)
      : led(ledPin), wakeupPin(wakeupPin) {}

  // Pair, bond and get the protocol ready. Drives the status LED.
  // Each attempt searches for at most 10 s; on a miss or a BLE glitch we retry
  // up to `attempts` times before giving up. A single failed wake would
  // otherwise mean a whole interval (an hour) of silence, so retrying here is
  // much cheaper than waiting for the next timer wake.
  bool connect(uint8_t attempts = 3)
  {
    led.begin();
    pairing.begin(MESH_BLE_MAC, MESH_BLE_PIN);

    for (uint8_t attempt = 1; attempt <= attempts; attempt++)
    {
      Serial.printf("Connect attempt %u/%u\n", attempt, attempts);
      if (attemptConnect())
        return true;
      if (attempt < attempts)
        delay(RETRY_DELAY_MS); // brief settle before re-scanning
    }

    return false;
  }

  // Send a text to MESH_DEST_ID (broadcast on MESH_CHANNEL, or direct to a node).
  void send(const char *text)
  {
    node.sendText(MESH_DEST_ID, text, destChannel());
  }

  // Send temp/humidity/pressure/voltage as a telemetry (EnvironmentMetrics) packet.
  void sendTelemetry(float temp, float humidity, float pressure, float voltage)
  {
    node.sendTelemetry(MESH_DEST_ID, temp, humidity, pressure, voltage, destChannel());
  }

  // Show the outcome LED for 10 s (green = sent, red = error), then go dark
  // and deep-sleep for the configured interval. A failed search sleeps at once.
  void sleepForInterval()
  {
    if (result == RESULT_CONNECTED || result == RESULT_ERROR)
      holdFor(PHASE_MS); // solid green or red, 10 s

    led.prepareForSleep(); // off + held low through deep sleep

    NimBLEClient *client = pairing.getClient();
    if (client && client->isConnected())
      client->disconnect();

    armAndSleep();
  }

  // Deep-sleep right away without touching the LED or BLE — used when we decide
  // not to send (e.g. a debounced PIR wake). The LED stays held dark from the
  // previous sleep.
  void sleepNow() { armAndSleep(); }

private:
  enum WakeResult
  {
    RESULT_NOT_FOUND,
    RESULT_CONNECTED,
    RESULT_ERROR,
  };

  static constexpr uint32_t PHASE_MS = 10000;          // search budget / LED hold
  static constexpr uint32_t CONFIG_TIMEOUT_MS = 10000; // give up on config dump after this
  static constexpr uint32_t RETRY_DELAY_MS = 500;      // settle time between connect attempts

  // One search-and-pair pass: scan up to PHASE_MS, and on a hit bond + open the
  // protocol. Sets `result` and returns true only when it's safe to send().
  bool attemptConnect()
  {
    pairing.startSearching();

    uint32_t start = millis();
    while (millis() - start < PHASE_MS)
    {
      led.update();
      switch (pairing.update())
      {
      case MeshPairing::PAIRED:
        led.setMode(StatusLed::PAIRED); // solid green
        led.update();
        if (!node.begin(pairing.getClient()))
        {
          led.setMode(StatusLed::PAIR_ERROR);
          result = RESULT_ERROR;
          return false;
        }
        waitForConfig();
        result = RESULT_CONNECTED;
        return true;

      case MeshPairing::FAILED:
        led.setMode(StatusLed::PAIR_ERROR); // solid red
        result = RESULT_ERROR;
        return false;

      case MeshPairing::SEARCHING:
      default:
        led.setMode(StatusLed::SEARCHING); // blue blinking
        break;
      }
      delay(10);
    }

    // Never found the node within the search window.
    result = RESULT_NOT_FOUND;
    return false;
  }

  // Request the node config and drain it until done (or a timeout); the node
  // accepts our packets once this handshake is underway.
  void waitForConfig()
  {
    node.requestConfig();
    uint32_t start = millis();
    while (!node.isComplete() && millis() - start < CONFIG_TIMEOUT_MS)
    {
      led.update();
      node.poll();
      delay(10);
    }
  }

  // Broadcast -> post to MESH_CHANNEL's feed; direct -> straight to the node.
  uint8_t destChannel() const
  {
    return (MESH_DEST_ID == MeshNode::BROADCAST_ADDR) ? MESH_CHANNEL : 0;
  }

  void holdFor(uint32_t ms)
  {
    uint32_t start = millis();
    while (millis() - start < ms)
    {
      led.update();
      delay(10);
    }
  }

  void armAndSleep()
  {
    Serial.printf("Deep sleeping for %d min...\n", MESH_SEND_INTERVAL_MIN);
    Serial.flush();
    delay(50);

    // Wake on either the interval timer, or motion (PIR drives wakeupPin HIGH).
    esp_sleep_enable_timer_wakeup((uint64_t)MESH_SEND_INTERVAL_MIN * 60ULL * 1000000ULL);
    esp_sleep_enable_ext1_wakeup(1ULL << wakeupPin, ESP_EXT1_WAKEUP_ANY_HIGH);
    esp_deep_sleep_start();
  }

  StatusLed led;
  uint8_t wakeupPin;
  MeshPairing pairing;
  MeshNode node;
  WakeResult result = RESULT_NOT_FOUND;
};
