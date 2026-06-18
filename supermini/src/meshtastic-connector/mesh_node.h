#pragma once
#include <NimBLEDevice.h>
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

  // Send a text message to another node (or broadcast) on the given channel.
  void sendText(uint32_t dest, const char *text, uint8_t channel = 0)
  {
    sendData(dest, channel, PORTNUM_TEXT_MESSAGE,
             (const uint8_t *)text, strlen(text), text);
  }

  // Send temperature/humidity/pressure/voltage as an EnvironmentMetrics telemetry packet.
  // NAN fields are omitted (e.g. no BMP280 -> no pressure).
  // Voltage is stuffed into the lux field (9) to avoid colliding with the parent node.
  void sendTelemetry(uint32_t dest, float temp, float humidity, float pressure,
                     float voltage, uint8_t channel = 0)
  {
    // EnvironmentMetrics{ temperature=1, relative_humidity=2, barometric_pressure=3, lux=9 }
    uint8_t envBuf[40];
    ProtoWriter env(envBuf, sizeof(envBuf));
    if (!isnan(temp))
      env.float32(1, temp);
    if (!isnan(humidity))
      env.float32(2, humidity);
    if (!isnan(pressure))
      env.float32(3, pressure);
    if (!isnan(voltage))
      env.float32(9, voltage);

    // Telemetry{ environment_metrics = 3 }
    uint8_t telBuf[48];
    ProtoWriter tel(telBuf, sizeof(telBuf));
    tel.message(3, env);

    sendData(dest, channel, PORTNUM_TELEMETRY, tel.data(), tel.size(), "telemetry");
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

  // Pre-seed the hop limit from config. Non-zero => fixed value, no need to read
  // the node's config. Zero => we'll learn it from the node's LoRa config.
  void setHopLimit(uint8_t h) { hopLimit = h; }

  // True once we have a hop limit to use (either pre-seeded or read from config).
  bool hopLimitKnown() const { return hopLimit != 0; }

private:
  // Meshtastic BLE GATT UUIDs
  static constexpr const char *SERVICE_UUID = "6ba1b218-15a8-461f-9fa8-5dcae273eafd";
  static constexpr const char *TORADIO_UUID = "f75c76d2-129e-4dad-a1dd-7866124401e7";
  static constexpr const char *FROMRADIO_UUID = "2c55e69e-4993-11ed-b878-0242ac120002";
  static constexpr const char *FROMNUM_UUID = "ed9da18c-a800-4f66-a670-aa7547e34453";

  static constexpr uint32_t CONFIG_NONCE = 0x1A2B3C4D;
  static constexpr int MAX_READS_PER_POLL = 64;
  static constexpr uint32_t PORTNUM_TEXT_MESSAGE = 1; // PortNum.TEXT_MESSAGE_APP
  static constexpr uint32_t PORTNUM_TELEMETRY = 67;   // PortNum.TELEMETRY_APP
  static constexpr uint32_t DEFAULT_HOPS = 3;         // used if we asked for config but it never arrived
  static constexpr uint32_t MAX_HOPS = 7;             // protocol ceiling

  // Build ToRadio{ MeshPacket{ Data{ portnum, payload } } } and write it.
  void sendData(uint32_t dest, uint8_t channel, uint32_t portnum,
                const uint8_t *payload, size_t payloadLen, const char *label)
  {
    // Data{ portnum, payload }
    uint8_t dataBuf[96];
    ProtoWriter data(dataBuf, sizeof(dataBuf));
    data.varint(1, portnum);
    data.bytes(2, payload, payloadLen);

    // MeshPacket{ to, channel, hop_limit, want_ack, decoded = Data }
    uint8_t pktBuf[128];
    ProtoWriter pkt(pktBuf, sizeof(pktBuf));
    bool broadcast = (dest == BROADCAST_ADDR);
    pkt.fixed32(2, dest);        // to (0xFFFFFFFF = channel broadcast)
    if (channel != 0)            // proto3 omits the zero default
      pkt.varint(3, channel);    // channel index
    uint32_t hops = hopLimit ? hopLimit : DEFAULT_HOPS;
    pkt.varint(9, hops);         // hop_limit (configured value, or read from node config)
    if (!broadcast)              // ack only makes sense for direct messages
      pkt.varint(10, 1);         // want_ack
    pkt.message(4, data);        // decoded

    // ToRadio{ packet = MeshPacket }
    uint8_t outBuf[160];
    ProtoWriter out(outBuf, sizeof(outBuf));
    out.message(1, pkt);

    bool ok = toRadio->writeValue(out.data(), out.size(), true);
    Serial.printf("MeshNode: sent %s to !%08x on ch%u (id=node-assigned) [%u bytes, ok=%d]\n",
                  label, dest, channel, (unsigned)out.size(), ok);
  }

  // FromRadio field numbers (payload_variant oneof)
  enum
  {
    F_MY_INFO = 3,
    F_NODE_INFO = 4,
    F_CONFIG = 5,
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
      else if (field == F_CONFIG && wt == 2)
      {
        size_t l;
        const uint8_t *b = r.readBytes(l);
        decodeConfig(b, l);
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

  // Config{ lora = 6 (LoRaConfig) } — we only care about the LoRa hop limit.
  void decodeConfig(const uint8_t *data, size_t len)
  {
    ProtoReader r(data, len);
    uint32_t field;
    uint8_t wt;
    while (r.nextField(field, wt))
    {
      if (field == 6 && wt == 2) // Config.lora
      {
        size_t l;
        const uint8_t *b = r.readBytes(l);
        decodeLoRaConfig(b, l);
      }
      else
        r.skip(wt);
    }
  }

  // LoRaConfig{ hop_limit = 8 }. Only adopt an explicit, in-range value; if it's
  // absent/0 we fall back to DEFAULT_HOPS at send time (sending 0 = local only).
  void decodeLoRaConfig(const uint8_t *data, size_t len)
  {
    ProtoReader r(data, len);
    uint32_t field;
    uint8_t wt;
    while (r.nextField(field, wt))
    {
      if (field == 8 && wt == 0) // hop_limit
      {
        uint32_t h = (uint32_t)r.readVarint();
        if (h > 0 && h <= MAX_HOPS)
          hopLimit = h;
        Serial.printf("  [LoRaConfig] hop_limit=%u (using %u)\n", h, hopLimit);
      }
      else
        r.skip(wt);
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
  uint8_t hopLimit = 0; // 0 = unknown; set via setHopLimit() or read from LoRa config
};
