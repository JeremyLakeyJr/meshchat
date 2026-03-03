# MeshChat — LoRa ESP32 Hybrid OS

A hybrid **Meshtastic + BitChat** firmware for ESP32 LoRa hardware.

- **LoRa mesh networking** inspired by [Meshtastic](https://meshtastic.org/) — long-range, multi-hop, flood-routed radio mesh.
- **Bluetooth LE chat** inspired by [BitChat](https://bitchat.app/) — peer-to-peer, encrypted local messaging over BLE.
- **WiFi** — optional AP/STA mode with MQTT bridge and OTA firmware updates.

## Supported Hardware

| Board | LoRa | Display | Keyboard | BLE | WiFi |
|---|---|---|---|---|---|
| Heltec WiFi LoRa 32 V3 | SX1262 | 128×64 OLED | — | ✓ | ✓ |
| Heltec WiFi LoRa 32 V2 | SX1276 | 128×64 OLED | — | ✓ | ✓ |
| LILYGO T-Deck | SX1262 | 320×240 TFT | ✓ (built-in) | ✓ | ✓ |

## Features

### Meshtastic-style LoRa Mesh
- Flood-and-forward routing with configurable hop limit (default 7)
- Duplicate-packet filter (ring buffer, 64 entries)
- Random back-off relay delay to reduce collisions
- Per-node SNR/RSSI tracking and stale-node pruning
- Node heartbeat broadcasts every 60 s
- Unified `MeshPacket` wire format (≤255 bytes, LoRa + BLE compatible)

### BitChat-style Bluetooth LE
- GATT server + client (NimBLE, low memory footprint)
- Continuous peripheral advertising; scans for peers every 5 s
- Same `MeshPacket` format bridged over BLE — messages propagate across both transports
- Up to 8 simultaneous BLE connections

### WiFi
- **Access Point mode** on boot (`MeshChat-XXYY` / password `meshchat`)
- **Station mode** for joining existing networks
- **MQTT bridge** (JSON-encoded packets to `meshchat/#` topic — bring your own broker)
- **Arduino OTA** for wireless firmware updates

### Display UI
- **Status bar**: LoRa / BLE / WiFi indicators + last RSSI + battery %
- **Chat screen**: scrolling message history with sender names and timestamps
- **Node list**: all known mesh nodes with RSSI, SNR, and hop count
- **Toast notifications** for incoming messages

## Project Structure

```
meshchat/
├── platformio.ini              # PlatformIO build config (all board targets)
└── src/
    ├── config.h                # Hardware pins, protocol constants, packet types
    ├── main.cpp                # Firmware entry point (setup + loop)
    ├── lora/
    │   ├── lora_handler.h/.cpp # LoRa radio init, TX/RX, relay
    ├── mesh/
    │   ├── mesh_router.h/.cpp  # Flood routing, dedup, node table
    ├── bluetooth/
    │   ├── ble_handler.h/.cpp  # BLE GATT server/client (NimBLE)
    ├── wifi/
    │   ├── wifi_handler.h/.cpp # WiFi AP/STA, MQTT bridge, OTA
    ├── display/
    │   ├── display_handler.h/.cpp  # OLED (SSD1306) + TFT (TFT_eSPI) UI
    └── chat/
        ├── chat_manager.h/.cpp # Message history, channels, routing dispatch
```

## Building & Flashing

### Prerequisites
- [PlatformIO](https://platformio.org/) (VS Code extension or CLI)

### Flash Heltec V3
```bash
pio run -e heltec_wifi_lora_32_V3 -t upload
```

### Flash Heltec V2
```bash
pio run -e heltec_wifi_lora_32_V2 -t upload
```

### Flash T-Deck
```bash
pio run -e lilygo_tdeck -t upload
```

### Monitor serial output
```bash
pio device monitor
```

## Configuration

Edit `src/config.h` to adjust:

| Constant | Default | Description |
|---|---|---|
| `LORA_FREQ` | `915E6` | Radio frequency (915 MHz US / 868 MHz EU) |
| `LORA_SPREADING_FACTOR` | `10` | SF7–SF12; higher = longer range, slower |
| `LORA_SYNC_WORD` | `0x34` | Network identifier (keep same across nodes) |
| `MAX_HOPS` | `7` | Maximum relay hops per packet |
| `HEARTBEAT_INTERVAL_MS` | `60000` | Heartbeat period (ms) |
| `NODE_TIMEOUT_MS` | `300000` | Node expiry time (ms) |
| `BLE_SCAN_INTERVAL_MS` | `5000` | BLE peer scan interval (ms) |

Per-board pin assignments are set in `platformio.ini` via `-D` build flags.

## WiFi / MQTT Setup

On first boot the device creates a WiFi AP (`MeshChat-XXYY`, password `meshchat`).

To connect to an existing network, add a call in `setup()`:
```cpp
wifiHandler.beginSTA("YourSSID", "YourPassword");
wifiHandler.connectMQTT("broker.example.com", 1883);
```

To integrate a full MQTT client add [PubSubClient](https://github.com/knolleary/pubsubclient)
to `lib_deps` and wire it into `WiFiHandler`.

## Sending Messages

**Via T-Deck keyboard**: type and press Enter.

**Via serial console** (any board):
```
pio device monitor
# then type a message and press Enter
```

**Via API** (in firmware):
```cpp
chatManager.sendMessage("Hello mesh!");
chatManager.sendPrivate("Secret", targetNodeID);
```

## Packet Format

All messages share a single 255-byte `MeshPacket` wire format regardless of transport:

```
Offset  Len  Field
0       1    version (= 1)
1       1    type (PacketType enum)
2       1    flags (transport + options)
3       4    from[4] — sender node ID (MAC-derived)
7       4    to[4]   — destination (FF:FF:FF:FF = broadcast)
11      2    packet_id — per-sender sequence number
13      1    hop_limit — remaining hops (countdown from hop_start)
14      1    hop_start — original hop limit
15      1    channel — logical channel index
16      1    payload_len
17      ≤220 payload
```

## License

MIT
