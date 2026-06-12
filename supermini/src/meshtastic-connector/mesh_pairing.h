#pragma once
#include <NimBLEDevice.h>

// Pairs (BLE bonds) with a Meshtastic node by MAC, using its fixed PIN.
//
// Flow:
//   begin()           -> init NimBLE + security (we act as the PIN keyboard)
//   startSearching()  -> async scan for the target MAC (non-blocking)
//   update()          -> call from loop(); once the node is found it connects
//                        and bonds, then reports PAIRED or FAILED
//
// The node must have Bluetooth pairing set to "Fixed PIN" matching MESH_BLE_PIN.
class MeshPairing
{
public:
  enum State
  {
    SEARCHING,
    PAIRED,
    FAILED,
  };

  void begin(const char *macStr, uint32_t pin)
  {
    targetMac = macStr;
    targetMac.toLowerCase();
    clientCb.pin = pin;

    NimBLEDevice::init("supermini-mesh");
    // NB: bonds live in NVS and survive deep sleep + power cycles. We keep them
    // on purpose so we only do the full PIN pairing once; later wakes re-encrypt
    // from the stored key (no PIN). A stale bond is handled in connectAndBond().
    Serial.printf("Stored BLE bonds: %d\n", NimBLEDevice::getNumBonds());
    // Meshtastic packets exceed the 23-byte default ATT MTU; ask for a big one
    // so a ToRadio write fits in a single ATT operation (like the phone app does).
    NimBLEDevice::setMTU(517);
    // bonding + MITM + secure connections; we input the PIN the node shows.
    NimBLEDevice::setSecurityAuth(true, true, true);
    NimBLEDevice::setSecurityIOCap(BLE_HS_IO_KEYBOARD_ONLY);
    NimBLEDevice::setSecurityPasskey(pin);

    scan = NimBLEDevice::getScan();
    scan->setScanCallbacks(&scanCb, false);
    scan->setActiveScan(true);
    scan->setInterval(100);
    scan->setWindow(80);
  }

  // Start (or restart) the async scan for the node.
  void startSearching()
  {
    state = SEARCHING;
    scanCb.targetMac = targetMac;
    scanCb.found = false;
    scan->start(0, false); // 0 = scan until we stop it
    Serial.printf("Searching for Meshtastic node %s ...\n", targetMac.c_str());
  }

  // Drive the flow; non-blocking while SEARCHING (scan runs async).
  State update()
  {
    if (state == SEARCHING && scanCb.found)
    {
      scan->stop();
      state = connectAndBond(scanCb.foundAddr) ? PAIRED : FAILED;
    }
    return state;
  }

  State getState() const { return state; }

  // Valid after PAIRED; the connected + bonded client to talk the protocol over.
  NimBLEClient *getClient() { return client; }

private:
  // -- scan callback: flags when the target MAC shows up --
  struct ScanCb : public NimBLEScanCallbacks
  {
    String targetMac;
    volatile bool found = false;
    NimBLEAddress foundAddr;

    void onResult(const NimBLEAdvertisedDevice *dev) override
    {
      if (found)
        return;
      String mac = dev->getAddress().toString().c_str();
      mac.toLowerCase();
      if (mac == targetMac)
      {
        foundAddr = dev->getAddress();
        found = true;
      }
    }
  };

  // -- client/security callback: injects the PIN on request --
  struct ClientCb : public NimBLEClientCallbacks
  {
    uint32_t pin = 0;

    void onConnect(NimBLEClient *) override { Serial.println("BLE link up"); }

    void onDisconnect(NimBLEClient *, int reason) override
    {
      Serial.printf("BLE link down, reason=%d\n", reason);
    }

    void onPassKeyEntry(NimBLEConnInfo &connInfo) override
    {
      Serial.printf("Node asked for PIN, sending %06u\n", pin);
      NimBLEDevice::injectPassKey(connInfo, pin);
    }

    void onAuthenticationComplete(NimBLEConnInfo &connInfo) override
    {
      Serial.printf("Auth complete: encrypted=%d bonded=%d\n",
                    connInfo.isEncrypted(), connInfo.isBonded());
    }
  };

  bool connectAndBond(const NimBLEAddress &addr)
  {
    Serial.printf("Found node %s, connecting...\n", addr.toString().c_str());

    client = NimBLEDevice::getClientByPeerAddress(addr);
    if (!client)
      client = NimBLEDevice::createClient();
    client->setClientCallbacks(&clientCb, false);

    if (!client->connect(addr))
    {
      Serial.println("Connect failed");
      NimBLEDevice::deleteClient(client);
      return false;
    }

    // Widen the supervision timeout so the SMP exchange can finish.
    client->updateConnParams(12, 12, 0, 400);
    delay(100);

    bool hadBond = NimBLEDevice::isBonded(addr);
    Serial.println(hadBond ? "Existing bond, re-encrypting..."
                           : "No bond, starting secure pairing...");

    // secureConnection() re-uses the stored bond when one exists (no PIN), or
    // runs the full PIN pairing otherwise. If a bond we thought was valid is
    // rejected, the node likely forgot us — drop it and pair fresh once.
    if (!client->secureConnection())
    {
      if (hadBond)
      {
        Serial.println("Stale bond rejected, deleting and re-pairing...");
        NimBLEDevice::deleteBond(addr);
        client->disconnect();
        delay(100);
        if (!client->connect(addr))
        {
          Serial.println("Reconnect failed");
          NimBLEDevice::deleteClient(client);
          return false;
        }
        client->updateConnParams(12, 12, 0, 400);
        delay(100);
        if (!client->secureConnection())
        {
          Serial.println("Pairing failed");
          client->disconnect();
          return false;
        }
      }
      else
      {
        Serial.println("Pairing failed");
        client->disconnect();
        return false;
      }
    }

    NimBLEConnInfo info = client->getConnInfo();
    bool ok = info.isEncrypted();
    Serial.printf("Link secure %s (encrypted=%d, bonded=%d)\n",
                  ok ? "SUCCESS" : "FAILED", info.isEncrypted(), info.isBonded());
    return ok;
  }

  NimBLEScan *scan = nullptr;
  NimBLEClient *client = nullptr;
  ScanCb scanCb;
  ClientCb clientCb;
  String targetMac;
  State state = SEARCHING;
};
