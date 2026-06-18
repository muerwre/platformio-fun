#pragma once

// BLE MAC of the Meshtastic node to pair with (lowercase or uppercase, with colons)
#define MESH_BLE_MAC "aa:bb:cc:dd:ee:ff"

// Fixed BLE pairing PIN configured on the Meshtastic node (Bluetooth -> Fixed PIN)
#define MESH_BLE_PIN 123456

// Where to send the "Hello" text:
//   0xFFFFFFFF      -> broadcast, shows up in the channel feed for everyone
//   <a node number> -> direct/private message to that node only (the !hex id without '!')
#define MESH_DEST_ID 0xFFFFFFFF

// Channel index to send on (0 = primary channel)
#define MESH_CHANNEL 0

// Minutes of deep sleep between messages (e.g. 60 = once per hour; use 1 for debugging)
#define MESH_SEND_INTERVAL_MIN 60

// Hop limit for outgoing packets (max 7). A fixed value lets us skip the slow
// node-config handshake entirely. Set to 0 to instead read the node's own
// LoRa hop_limit on connect (adds the config dump's latency).
#define MESH_HOP_LIMIT 7
