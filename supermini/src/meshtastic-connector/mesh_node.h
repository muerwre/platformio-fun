#pragma once
#include <NimBLEDevice.h>
#include <esp_random.h>
#include "proto_reader.h"
#include "proto_writer.h"

// Talks the Meshtastic BLE protocol over an already-bonded client.
//
// Usage:
//   node.begin(pairing.getClient());  // discover chars, subscribe to fromNum
//   node.requestConfig();             // ask the node to stream its config
//   node.poll();                      // call from loop(); drains + decodes packets
//
// Decodes a useful subset of FromRadio (MyNodeInfo, NodeInfo, DeviceMetadata)
// and logs it. Stops at config_complete_id.
class MeshNode
{
public:
  static constexpr uint32_t BROADCAST_ADDR = 0xFFFFFFFF; // post to channel feed

  bool begin(NimBLEClient *client)
  {
    this->client = client;
    if (!client || !client->isConnected())
    {
      Serial.println("MeshNode: no connected client");
      return false;
    }

    NimBLERemoteService *svc = client->getService(SERVICE_UUID);
    if (!svc)
    {
      Serial.println("MeshNode: Meshtastic service not found");
      return false;
    }

    toRadio = svc->getCharacteristic(TORADIO_UUID);
    fromRadio = svc->getCharacteristic(FROMRADIO_UUID);
    fromNum = svc->getCharacteristic(FROMNUM_UUID);
    if (!toRadio || !fromRadio)
    {
      Serial.println("MeshNode: required characteristics missing");
      return false;
    }

    // fromNum notifies us whenever the node has new packets queued.
    if (fromNum && fromNum->canNotify())
      fromNum->subscribe(true, [this](NimBLERemoteCharacteristic *, uint8_t *, size_t, bool)
                         { dataReady = true; });

    Serial.printf("MeshNode: connected (MTU=%d, toRadio write=%d writeNR=%d)\n",
                  client->getMTU(), toRadio->canWrite(), toRadio->canWriteNoResponse());
    return true;
  }

  // Ask the node to dump its config + node DB (MyNodeInfo, NodeInfo..., metadata).
  void requestConfig()
  {
    complete = false;
    dataReady = true; // force an initial drain

    // ToRadio{ want_config_id = nonce } -> field 3, varint
    uint8_t buf[8];
    ProtoWriter w(buf, sizeof(buf));
    w.varint(3, CONFIG_NONCE);

    Serial.println("MeshNode: requesting config...");
    toRadio->writeValue(w.data(), w.size(), true);
  }

  // Send a text message to another node (its node number, e.g. 0x433a1b2c)
  // on the given channel index (0 = primary).
  void sendText(uint32_t dest, const char *text, uint8_t channel = 0)
  {
    // Data{ portnum = TEXT_MESSAGE_APP, payload = text }
    uint8_t dataBuf[64];
    ProtoWriter data(dataBuf, sizeof(dataBuf));
    data.varint(1, PORTNUM_TEXT_MESSAGE); // portnum
    data.string(2, text);                 // payload

    // MeshPacket{ to, channel, id, hop_limit, want_ack, decoded = Data }
    uint8_t pktBuf[96];
    ProtoWriter pkt(pktBuf, sizeof(pktBuf));
    bool broadcast = (dest == BROADCAST_ADDR);
    uint32_t id = esp_random(); // must be unique per message — the node drops
    if (id == 0)                // duplicate ids as "already seen recently"
      id = 1;
    pkt.fixed32(2, dest);        // to (0xFFFFFFFF = channel broadcast)
    if (channel != 0)            // proto3 omits the zero default
      pkt.varint(3, channel);    // channel index
    pkt.fixed32(6, id);          // packet id (random, non-zero)
    pkt.varint(9, DEFAULT_HOPS); // hop_limit
    if (!broadcast)              // ack only makes sense for direct messages
      pkt.varint(10, 1);         // want_ack
    pkt.message(4, data);        // decoded

    // ToRadio{ packet = MeshPacket }
    uint8_t outBuf[128];
    ProtoWriter out(outBuf, sizeof(outBuf));
    out.message(1, pkt);

    bool ok = toRadio->writeValue(out.data(), out.size(), true);
    Serial.printf("MeshNode: sent \"%s\" to !%08x on ch%u (id=0x%08x) [%u bytes, ok=%d]\n",
                  text, dest, channel, id, (unsigned)out.size(), ok);
  }

  // Drain and decode whatever the node has queued. Call often from loop().
  void poll()
  {
    if (!dataReady || complete)
      return;
    dataReady = false;

    // Each read pops one FromRadio packet; empty read => nothing left for now.
    for (int i = 0; i < MAX_READS_PER_POLL; i++)
    {
      NimBLEAttValue val = fromRadio->readValue();
      if (val.size() == 0)
        break;
      decodeFromRadio(val.data(), val.size());
      if (complete)
        break;
    }
  }

  bool isComplete() const { return complete; }

private:
  // Meshtastic BLE GATT UUIDs
  static constexpr const char *SERVICE_UUID = "6ba1b218-15a8-461f-9fa8-5dcae273eafd";
  static constexpr const char *TORADIO_UUID = "f75c76d2-129e-4dad-a1dd-7866124401e7";
  static constexpr const char *FROMRADIO_UUID = "2c55e69e-4993-11ed-b878-0242ac120002";
  static constexpr const char *FROMNUM_UUID = "ed9da18c-a800-4f66-a670-aa7547e34453";

  static constexpr uint32_t CONFIG_NONCE = 0x1A2B3C4D;
  static constexpr int MAX_READS_PER_POLL = 64;
  static constexpr uint32_t PORTNUM_TEXT_MESSAGE = 1; // PortNum.TEXT_MESSAGE_APP
  static constexpr uint32_t DEFAULT_HOPS = 3;

  // FromRadio field numbers (payload_variant oneof)
  enum
  {
    F_MY_INFO = 3,
    F_NODE_INFO = 4,
    F_CONFIG_COMPLETE_ID = 7,
    F_METADATA = 13,
  };

  void decodeFromRadio(const uint8_t *data, size_t len)
  {
    ProtoReader r(data, len);
    uint32_t field;
    uint8_t wt;
    while (r.nextField(field, wt))
    {
      if (field == F_MY_INFO && wt == 2)
      {
        size_t l;
        const uint8_t *b = r.readBytes(l);
        decodeMyNodeInfo(b, l);
      }
      else if (field == F_NODE_INFO && wt == 2)
      {
        size_t l;
        const uint8_t *b = r.readBytes(l);
        decodeNodeInfo(b, l);
      }
      else if (field == F_METADATA && wt == 2)
      {
        size_t l;
        const uint8_t *b = r.readBytes(l);
        decodeMetadata(b, l);
      }
      else if (field == F_CONFIG_COMPLETE_ID && wt == 0)
      {
        r.readVarint();
        complete = true;
        Serial.println("MeshNode: config complete");
      }
      else
      {
        r.skip(wt);
      }
    }
  }

  // MyNodeInfo{ my_node_num = 1 }
  void decodeMyNodeInfo(const uint8_t *data, size_t len)
  {
    ProtoReader r(data, len);
    uint32_t field;
    uint8_t wt;
    uint32_t myNum = 0;
    while (r.nextField(field, wt))
    {
      if (field == 1 && wt == 0)
        myNum = (uint32_t)r.readVarint();
      else
        r.skip(wt);
    }
    Serial.printf("  [MyNodeInfo] my_node_num=%u (!%08x)\n", myNum, myNum);
  }

  // NodeInfo{ num = 1, user = 2 (User) }
  void decodeNodeInfo(const uint8_t *data, size_t len)
  {
    ProtoReader r(data, len);
    uint32_t field;
    uint8_t wt;
    uint32_t num = 0;
    String longName, shortName, id;
    uint32_t hwModel = 0;

    while (r.nextField(field, wt))
    {
      if (field == 1 && wt == 0)
        num = (uint32_t)r.readVarint();
      else if (field == 2 && wt == 2)
      {
        size_t l;
        const uint8_t *b = r.readBytes(l);
        decodeUser(b, l, id, longName, shortName, hwModel);
      }
      else
        r.skip(wt);
    }
    Serial.printf("  [NodeInfo] num=!%08x id=%s name=\"%s\" (%s) hw=%u\n",
                  num, id.c_str(), longName.c_str(), shortName.c_str(), hwModel);
  }

  // User{ id = 1, long_name = 2, short_name = 3, hw_model = 5 }
  void decodeUser(const uint8_t *data, size_t len,
                  String &id, String &longName, String &shortName, uint32_t &hwModel)
  {
    ProtoReader r(data, len);
    uint32_t field;
    uint8_t wt;
    while (r.nextField(field, wt))
    {
      if (field == 1 && wt == 2)
        id = r.readString();
      else if (field == 2 && wt == 2)
        longName = r.readString();
      else if (field == 3 && wt == 2)
        shortName = r.readString();
      else if (field == 5 && wt == 0)
        hwModel = (uint32_t)r.readVarint();
      else
        r.skip(wt);
    }
  }

  // DeviceMetadata{ firmware_version = 1, has_bluetooth = 5, hw_model = 9 }
  void decodeMetadata(const uint8_t *data, size_t len)
  {
    ProtoReader r(data, len);
    uint32_t field;
    uint8_t wt;
    String fw;
    uint32_t hwModel = 0;
    while (r.nextField(field, wt))
    {
      if (field == 1 && wt == 2)
        fw = r.readString();
      else if (field == 9 && wt == 0)
        hwModel = (uint32_t)r.readVarint();
      else
        r.skip(wt);
    }
    Serial.printf("  [Metadata] firmware=%s hw=%u\n", fw.c_str(), hwModel);
  }

  NimBLEClient *client = nullptr;
  NimBLERemoteCharacteristic *toRadio = nullptr;
  NimBLERemoteCharacteristic *fromRadio = nullptr;
  NimBLERemoteCharacteristic *fromNum = nullptr;
  volatile bool dataReady = false;
  bool complete = false;
};
