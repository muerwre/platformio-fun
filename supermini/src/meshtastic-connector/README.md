## Meshtastic connector

Both ESP32-Supermini and meshtastic node has bluetooth, so, they can talk by serial using .proto 
files. The idea of this project is to make esp32 send it's data through meshtastic node.

### Step 1 — BLE pairing (this sketch)

Bond the Supermini (BLE client) with the Meshtastic node over BLE using the node's
fixed PIN. On the node set `Bluetooth -> Pairing mode: Fixed PIN` and use that PIN here.

Copy `secrets.example.h` to `secrets.h` and fill in:

- `MESH_BLE_MAC` — the node's BLE MAC
- `MESH_BLE_PIN` — the node's fixed PIN
- `MESH_DEST_ID` — node number of the peer to greet (the `!hex` id, without `!`)

LED (onboard WS2812 on GPIO8):

- **blue blinking** — searching for the node
- **green blinking** — paired / bonded
- **red solid** — pairing error, retries after 5 s

Progress is logged to serial at 115200.

### Step 2 — read node info

After bonding we open the Meshtastic GATT service and write a `want_config_id`
request. The node streams back `MyNodeInfo`, a `NodeInfo` per known node and
`DeviceMetadata`, terminated by `config_complete_id`. We hand-decode the protobuf
wire format (no nanopb) and log node numbers, names and firmware version.

### Step 3 — send a text

Once the node DB is read, we send a `"Hello"` text message to `MESH_DEST_ID`
(`ToRadio{ packet: MeshPacket{ to, decoded: Data{ portnum=TEXT_MESSAGE_APP } } }`).

### Files

- `main.cpp` — state machine (search → pair → read info → say hello → retry)
- `status_led.h` — non-blocking RGB LED states
- `mesh_pairing.h` — NimBLE scan + connect + PIN bonding
- `mesh_node.h` — Meshtastic GATT protocol: request config, decode info, send text
- `proto_reader.h` / `proto_writer.h` — minimal protobuf wire-format codec
- `secrets.h` — node MAC, PIN and dest id (git-ignored)

### Finding device (in linux)

```
bluetoothctl
scan on
# locate device mac, put it in secrets.h
scan off
```