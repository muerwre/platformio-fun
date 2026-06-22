#pragma once
#include <Arduino.h>
#include <esp_sleep.h>
#include <sys/time.h>
#include "status_led.h"
#include "mesh_pairing.h"
#include "mesh_node.h"
#include "telemetry.h"

// Boilerplate for a battery-powered Meshtastic sensor node.
//
// One MeshConnector owns the whole deep-sleep duty cycle so an app's main.cpp is
// just declarative config:
//
//   MeshConnector mesh(LED_PIN);
//
//   void setup() {
//     mesh.setNode(MESH_BLE_MAC, MESH_BLE_PIN);     // who to pair with
//     mesh.setDestination(MESH_DEST_ID, MESH_CHANNEL);
//     mesh.setHopLimit(MESH_HOP_LIMIT);
//     mesh.setTelemetryInterval(MESH_SEND_INTERVAL_MIN * 60);
//     mesh.setWakeUpMode(HIGH);                     // ext1 level for all triggers
//     mesh.addWakeupTrigger(PIR_PIN, "Main Entrance Open", 5 /*min cooldown*/);
//     mesh.onTelemetry(readTelemetry);              // optional sensor callback
//     mesh.run();                                   // never returns; deep-sleeps
//   }
//   void loop() {}
//
// Both halves are optional: a node with no triggers is a plain periodic telemetry
// beacon; a node with no telemetry provider is a pure event detector.
//
// On each wake run() decides what happened (a trigger pin fired, or the telemetry
// timer elapsed), opens one BLE session to send everything that's due, then deep-
// sleeps until the next telemetry deadline (or until a trigger pin fires sooner).
class MeshConnector
{
public:
  static constexpr uint8_t MAX_TRIGGERS = 4;

  // ledPin: onboard WS2812 status LED.
  explicit MeshConnector(uint8_t ledPin) : led(ledPin) {}

  // --- Configuration (call from setup() before run()) ---

  // BLE MAC + fixed PIN of the Meshtastic node to pair with.
  void setNode(const char *bleMac, uint32_t blePin)
  {
    nodeMac = bleMac;
    nodePin = blePin;
  }

  // Where outgoing packets go: destId 0xFFFFFFFF broadcasts on `channel`'s feed;
  // a node number sends a direct message (channel is then ignored).
  void setDestination(uint32_t destId, uint8_t channel)
  {
    destId_ = destId;
    channel_ = channel;
  }

  // Outgoing hop limit (1..7). 0 => read the node's own LoRa hop_limit on connect
  // (adds the config-dump latency; see MeshNode).
  void setHopLimit(uint8_t hops) { hopLimit = hops; }

  // Seconds of deep sleep between telemetry sends. 0 disables periodic telemetry.
  void setTelemetryInterval(uint32_t seconds) { intervalSec = seconds; }

  // ext1 wake level shared by every trigger pin: HIGH (default) or LOW. The
  // hardware allows only one level for all deep-sleep pins, so this is global.
  void setWakeUpMode(uint8_t level) { wakeHigh = (level != LOW); }

  // Register a pin whose assertion wakes the node and sends a detection event
  // carrying `remark` (e.g. "Main Entrance Open"). Repeat assertions within
  // `cooldownMinutes` are suppressed so a busy sensor can't spam the mesh.
  void addWakeupTrigger(uint8_t pin, const char *remark, uint16_t cooldownMinutes = 5)
  {
    if (triggerCount >= MAX_TRIGGERS)
    {
      Serial.printf("MeshConnector: ignoring trigger on pin %u (max %u)\n", pin, MAX_TRIGGERS);
      return;
    }
    triggers[triggerCount] = {pin, remark, (uint32_t)cooldownMinutes * 60};
    triggerCount++;
  }

  // Provider invoked (before BLE starts) to gather a telemetry sample to send.
  void onTelemetry(TelemetryProvider provider) { telemetryProvider = provider; }

  // --- The duty cycle ---

  // Run one wake cycle to completion, then deep-sleep. Never returns.
  void run()
  {
    int64_t now = secondsSinceStart();

    // nextTelemetrySec == 0 means we've never sent telemetry: this is the cold
    // boot (or the device was just reflashed). Announce ourselves immediately —
    // like the pre-refactor firmware did — then settle into the interval.
    bool firstBoot = (nextTelemetrySec == 0 && intervalSec > 0);
    if (firstBoot)
      nextTelemetrySec = now + intervalSec;

    Serial.printf("Woke at %llds (%llds since last wake), telemetry due at %llds%s\n",
                  (long long)now, (long long)(now - lastWakeSec), (long long)nextTelemetrySec,
                  firstBoot ? " (first boot)" : "");
    lastWakeSec = now;

    // Which configured trigger pins actually caused (or are asserting at) this
    // wake, minus any still inside their cooldown window.
    uint8_t fired[MAX_TRIGGERS];
    uint8_t firedCount = collectActiveTriggers(now, fired);

    bool telemetryDue = intervalSec > 0 && (firstBoot || now >= nextTelemetrySec);

    // Nothing to do this wake (e.g. a debounced re-trigger, no telemetry due):
    // go straight back to sleep without spinning up the radio. We never touch
    // nextTelemetrySec here, so a trigger storm can't push the schedule back.
    if (firedCount == 0 && !telemetryDue)
    {
      Serial.println("Nothing due; sleeping.");
      sleepNow(secondsUntilTelemetry(now));
      return;
    }

    // Gather telemetry BEFORE BLE — I2C and NimBLE conflict on the C6 if the bus
    // is still live when the radio inits. We piggyback telemetry onto any wake
    // that opens a session, so a trigger wake also refreshes the periodic data.
    Telemetry sample;
    bool haveSample = false;
    if (telemetryProvider)
      haveSample = telemetryProvider(sample);

    if (connect())
    {
      for (uint8_t i = 0; i < firedCount; i++)
      {
        Trigger &t = triggers[fired[i]];
        node.sendDetection(destId_, t.remark, destChannel());
        t.lastFiredSec = now;
        t.firedOnce = true;
      }

      if (haveSample)
      {
        node.sendTelemetry(destId_, sample, destChannel());
        nextTelemetrySec = now + intervalSec;
        Serial.printf("Sent telemetry; next due at %llds\n", (long long)nextTelemetrySec);
      }
      else if (firedCount == 0 && telemetryProvider)
      {
        // A plain timer wake whose sensor read failed — flag it rather than
        // silently skipping, so a dead sensor is visible on the mesh.
        node.sendText(destId_, "sensor error", destChannel());
      }
    }

    sleepForInterval(secondsUntilTelemetry(now));
  }

private:
  // A configured wake source. cooldownSec/lastFiredSec/firedOnce debounce repeats.
  struct Trigger
  {
    uint8_t pin;
    const char *remark;
    uint32_t cooldownSec;
    int64_t lastFiredSec = 0; // synced from RTC each run, written back on send
    bool firedOnce = false;
  };

  static constexpr uint32_t PHASE_MS = 10000;          // search budget / LED hold
  static constexpr uint32_t CONFIG_TIMEOUT_MS = 10000; // give up on config dump after this
  static constexpr uint32_t RETRY_DELAY_MS = 500;      // settle time between connect attempts
  static constexpr int64_t MIN_RETRY_SEC = 60;         // on a failed due-send, retry soon

  // --- Wall clock (monotonic seconds since first boot; survives deep sleep) ---

  int64_t secondsSinceStart()
  {
    struct timeval tv = {0, 0};
    if (!timeStarted)
    {
      settimeofday(&tv, nullptr); // start the clock at zero on first boot
      timeStarted = true;
      return 0;
    }
    gettimeofday(&tv, nullptr);
    return tv.tv_sec;
  }

  // Seconds to sleep so we wake at the telemetry deadline (or sooner on a trigger),
  // clamped to a sane range. With no telemetry configured we just idle-wait.
  uint32_t secondsUntilTelemetry(int64_t now)
  {
    if (intervalSec == 0)
      return 3600; // no periodic telemetry: idle wait, triggers can still wake us
    int64_t remaining = nextTelemetrySec - now;
    if (remaining < MIN_RETRY_SEC)
      remaining = MIN_RETRY_SEC;
    if (remaining > (int64_t)intervalSec)
      remaining = intervalSec;
    return (uint32_t)remaining;
  }

  // --- Trigger detection + debounce ---

  // Pull each trigger's persisted debounce state out of RTC memory into the live
  // Trigger structs (they're rebuilt fresh by addWakeupTrigger() every boot).
  void syncTriggerState()
  {
    for (uint8_t i = 0; i < triggerCount; i++)
    {
      triggers[i].lastFiredSec = triggerState[i].lastFiredSec;
      triggers[i].firedOnce = triggerState[i].firedOnce;
    }
  }

  void persistTriggerState()
  {
    for (uint8_t i = 0; i < triggerCount; i++)
    {
      triggerState[i].lastFiredSec = triggers[i].lastFiredSec;
      triggerState[i].firedOnce = triggers[i].firedOnce;
    }
  }

  // Fills `out` with the indices of triggers that fired this wake and are past
  // their cooldown; returns how many.
  uint8_t collectActiveTriggers(int64_t now, uint8_t out[MAX_TRIGGERS])
  {
    syncTriggerState();

    // ext1 reports a bitmask of the pins that caused the wake. On a timer wake
    // (or cold boot) it's 0 and nothing is treated as fired.
    uint64_t mask = 0;
    if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT1)
      mask = esp_sleep_get_ext1_wakeup_status();

    uint8_t n = 0;
    for (uint8_t i = 0; i < triggerCount; i++)
    {
      if (!(mask & (1ULL << triggers[i].pin)))
        continue;
      Trigger &t = triggers[i];
      bool suppressed = t.firedOnce && (now - t.lastFiredSec) < (int64_t)t.cooldownSec;
      if (suppressed)
      {
        Serial.printf("Trigger pin %u suppressed (%llds < %us cooldown)\n",
                      t.pin, (long long)(now - t.lastFiredSec), t.cooldownSec);
        continue;
      }
      out[n++] = i;
    }
    return n;
  }

  // --- BLE connect (drives the status LED) ---

  bool connect(uint8_t attempts = 3)
  {
    led.begin();
    pairing.begin(nodeMac, nodePin);
    node.setHopLimit(hopLimit); // non-zero => skip config; 0 => read it from the node

    for (uint8_t attempt = 1; attempt <= attempts; attempt++)
    {
      Serial.printf("Connect attempt %u/%u\n", attempt, attempts);
      if (attemptConnect())
        return true;
      if (attempt < attempts)
        delay(RETRY_DELAY_MS);
    }
    return false;
  }

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
        led.setMode(StatusLed::PAIRED);
        led.update();
        if (!node.begin(pairing.getClient()))
        {
          led.setMode(StatusLed::PAIR_ERROR);
          result = RESULT_ERROR;
          return false;
        }
        if (!node.hopLimitKnown())
          waitForConfig();
        result = RESULT_CONNECTED;
        return true;

      case MeshPairing::FAILED:
        led.setMode(StatusLed::PAIR_ERROR);
        result = RESULT_ERROR;
        return false;

      case MeshPairing::SEARCHING:
      default:
        led.setMode(StatusLed::SEARCHING);
        break;
      }
      delay(10);
    }

    result = RESULT_NOT_FOUND;
    return false;
  }

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

  // Broadcast -> post to the configured channel's feed; direct -> straight to node.
  uint8_t destChannel() const
  {
    return (destId_ == MeshNode::BROADCAST_ADDR) ? channel_ : 0;
  }

  // --- Sleep ---

  // Show the outcome LED (green = sent, red = error) for PHASE_MS, then deep-sleep.
  void sleepForInterval(uint32_t timerSeconds)
  {
    if (result == RESULT_CONNECTED || result == RESULT_ERROR)
      holdFor(PHASE_MS);

    led.prepareForSleep();

    NimBLEClient *client = pairing.getClient();
    if (client && client->isConnected())
      client->disconnect();

    armAndSleep(timerSeconds);
  }

  // Deep-sleep immediately without touching the LED or BLE (used when we decide
  // not to send). The LED stays held dark from the previous sleep.
  void sleepNow(uint32_t timerSeconds) { armAndSleep(timerSeconds); }

  void holdFor(uint32_t ms)
  {
    uint32_t start = millis();
    while (millis() - start < ms)
    {
      led.update();
      delay(10);
    }
  }

  void armAndSleep(uint32_t timerSeconds)
  {
    persistTriggerState();

    if (timerSeconds < 1)
      timerSeconds = 1;
    Serial.printf("Deep sleeping for %lu s...\n", (unsigned long)timerSeconds);
    Serial.flush();
    delay(50);

    esp_sleep_enable_timer_wakeup((uint64_t)timerSeconds * 1000000ULL);

    uint64_t pinMask = 0;
    for (uint8_t i = 0; i < triggerCount; i++)
      pinMask |= (1ULL << triggers[i].pin);
    if (pinMask)
      esp_sleep_enable_ext1_wakeup(
          pinMask, wakeHigh ? ESP_EXT1_WAKEUP_ANY_HIGH : ESP_EXT1_WAKEUP_ANY_LOW);

    esp_deep_sleep_start();
  }

  enum WakeResult
  {
    RESULT_NOT_FOUND,
    RESULT_CONNECTED,
    RESULT_ERROR,
  };

  // Persistent debounce state, one slot per trigger index. Indices are stable
  // because addWakeupTrigger() runs in the same order every boot.
  struct TriggerState
  {
    int64_t lastFiredSec;
    bool firedOnce;
  };

  StatusLed led;
  MeshPairing pairing;
  MeshNode node;
  WakeResult result = RESULT_NOT_FOUND;

  // Config
  const char *nodeMac = nullptr;
  uint32_t nodePin = 0;
  uint32_t destId_ = MeshNode::BROADCAST_ADDR;
  uint8_t channel_ = 0;
  uint8_t hopLimit = 0;
  uint32_t intervalSec = 0;
  bool wakeHigh = true;
  TelemetryProvider telemetryProvider = nullptr;

  Trigger triggers[MAX_TRIGGERS];
  uint8_t triggerCount = 0;

  // RTC-backed state that must survive deep sleep. Declared static so it lands in
  // RTC slow memory; there is one MeshConnector per app, so a single set is fine.
  static RTC_DATA_ATTR bool timeStarted;
  static RTC_DATA_ATTR int64_t lastWakeSec;
  static RTC_DATA_ATTR int64_t nextTelemetrySec;
  static RTC_DATA_ATTR TriggerState triggerState[MAX_TRIGGERS];
};

// Definitions for the RTC-backed members (header-only lib, single instance).
RTC_DATA_ATTR bool MeshConnector::timeStarted = false;
RTC_DATA_ATTR int64_t MeshConnector::lastWakeSec = 0;
RTC_DATA_ATTR int64_t MeshConnector::nextTelemetrySec = 0;
RTC_DATA_ATTR MeshConnector::TriggerState
    MeshConnector::triggerState[MeshConnector::MAX_TRIGGERS] = {};
