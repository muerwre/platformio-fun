#ifndef ZIGBEE_MODE_ED
#error "Zigbee end device mode is not selected. Add -D ZIGBEE_MODE_ED to build_flags."
#endif

#include "Zigbee.h"
#include "ep/ZigbeeAnalog.h"

#define ZIGBEE_ENDPOINT 1

ZigbeeAnalog zbNonce(ZIGBEE_ENDPOINT);

static void nonce_task(void *arg)
{
  uint32_t nonce = 0;
  for (;;)
  {
    nonce++;
    Serial.printf("Nonce: %u\n", nonce);
    zbNonce.setAnalogInput((float)nonce);
    zbNonce.reportAnalogInput();
    delay(1000);
  }
}

void setup()
{
  Serial.begin(115200);

  zbNonce.setManufacturerAndModel("ESP32C6", "NoncePublisher");
  zbNonce.addAnalogInput();

  Zigbee.addEndpoint(&zbNonce);

  Serial.println("Starting Zigbee End Device...");
  if (!Zigbee.begin())
  {
    Serial.println("Zigbee init failed — rebooting");
    delay(1000);
    ESP.restart();
  }
  Serial.println("Zigbee started. Waiting to join network...");

  while (!Zigbee.connected())
  {
    delay(200);
    Serial.print(".");
  }
  Serial.println("\nJoined Zigbee network!");

  // Must be called after Zigbee.begin() — the Zigbee lock must exist first
  // min=1s, max=60s, report if value changes by 1.0
  zbNonce.setAnalogInputReporting(1, 60, 1.0f);

  xTaskCreate(nonce_task, "nonce_task", 2048, NULL, 10, NULL);
}

void loop()
{
  delay(100);
}
