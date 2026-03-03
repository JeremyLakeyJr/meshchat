#pragma once

// ============================================================
// MeshChat - Hardware & Protocol Configuration
// Supports: Heltec WiFi LoRa 32 V2/V3 and LILYGO T-Deck
// ============================================================

#include <Arduino.h>

// ----------------------------------------------------------
// Device identity (can be overridden per-board in platformio.ini)
// ----------------------------------------------------------
#ifndef DEVICE_NAME
  #define DEVICE_NAME "MeshChat"
#endif

// ----------------------------------------------------------
// LoRa radio defaults (overridden per-board in platformio.ini)
// ----------------------------------------------------------
#ifndef LORA_FREQ
  #define LORA_FREQ   915E6    // 915 MHz (US); use 868E6 for EU
#endif
#ifndef LORA_BANDWIDTH
  #define LORA_BANDWIDTH  125E3
#endif
#ifndef LORA_SPREADING_FACTOR
  #define LORA_SPREADING_FACTOR  10
#endif
#ifndef LORA_CODING_RATE
  #define LORA_CODING_RATE  5
#endif
#ifndef LORA_SYNC_WORD
  #define LORA_SYNC_WORD  0x34   // MeshChat network identifier
#endif
#ifndef LORA_TX_POWER
  #define LORA_TX_POWER   14     // dBm (max 20 for SX1276)
#endif

// ----------------------------------------------------------
// Mesh protocol constants
// ----------------------------------------------------------
#define MESHCHAT_VERSION        1
#define MAX_HOPS                7      // maximum mesh relay hops
#define FLOOD_REBROADCAST_DELAY 50     // ms random delay before rebroadcast
#define NODE_ID_LEN             4      // bytes in a node ID
#define PACKET_HISTORY_SIZE     64     // remembered packet IDs (duplicate filter)
#define HEARTBEAT_INTERVAL_MS   60000  // node heartbeat every 60 s
#define NODE_TIMEOUT_MS         300000 // remove node after 5 min of silence

// ----------------------------------------------------------
// BLE (BitChat) constants
// ----------------------------------------------------------
#define BLE_SERVICE_UUID       "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define BLE_RX_CHAR_UUID       "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define BLE_TX_CHAR_UUID       "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"
#define BLE_SCAN_INTERVAL_MS   5000    // scan for peers every 5 s
#define BLE_MAX_PACKET_SIZE    512     // bytes per BLE write

// ----------------------------------------------------------
// WiFi / MQTT bridge
// ----------------------------------------------------------
#define WIFI_CONNECT_TIMEOUT_MS  10000
#define MQTT_PORT                1883
#define MQTT_TOPIC_ROOT          "meshchat"

// ----------------------------------------------------------
// Display
// ----------------------------------------------------------
#define OLED_WIDTH  128
#define OLED_HEIGHT 64
#define TFT_WIDTH   320
#define TFT_HEIGHT  240
#define UI_REFRESH_MS  200   // display refresh rate

// ----------------------------------------------------------
// Packet types (shared across LoRa and BLE transports)
// ----------------------------------------------------------
enum class PacketType : uint8_t {
    TEXT_MESSAGE   = 0x01,   // plain text chat
    PRIVATE_MSG    = 0x02,   // encrypted direct message
    HEARTBEAT      = 0x10,   // node presence announcement
    NODE_INFO      = 0x11,   // node metadata (name, position)
    POSITION       = 0x12,   // GPS coordinates
    TELEMETRY      = 0x13,   // battery / environment data
    ROUTE_REQUEST  = 0x20,   // mesh route discovery
    ROUTE_REPLY    = 0x21,   // mesh route reply
    ACK            = 0x30,   // message acknowledgement
    CHANNEL_INFO   = 0x40,   // channel key exchange
    ADMIN          = 0x50,   // administrative command
};

// ----------------------------------------------------------
// Transport flags
// ----------------------------------------------------------
enum class TransportFlags : uint8_t {
    NONE       = 0x00,
    VIA_LORA   = 0x01,
    VIA_BLE    = 0x02,
    VIA_WIFI   = 0x04,
    ENCRYPTED  = 0x08,
    WANTS_ACK  = 0x10,
};

inline TransportFlags operator|(TransportFlags a, TransportFlags b) {
    return static_cast<TransportFlags>(
        static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}
inline bool operator&(TransportFlags a, TransportFlags b) {
    return static_cast<uint8_t>(a) & static_cast<uint8_t>(b);
}

// ----------------------------------------------------------
// Unified mesh packet (fits inside a single LoRa frame or BLE write)
// ----------------------------------------------------------
struct __attribute__((packed)) MeshPacket {
    uint8_t     version;          // protocol version
    PacketType  type;             // message type
    TransportFlags flags;         // transport / option flags
    uint8_t     from[NODE_ID_LEN];// sender node ID
    uint8_t     to[NODE_ID_LEN];  // destination (FF:FF:FF:FF = broadcast)
    uint16_t    packet_id;        // unique packet identifier (per sender)
    uint8_t     hop_limit;        // remaining relay hops (countdown)
    uint8_t     hop_start;        // original hop limit (for SNR ranking)
    uint8_t     channel;          // logical channel / key index
    uint8_t     payload_len;      // length of payload that follows
    // Max payload is 220 bytes to stay within LoRa's 255-byte frame limit
    // (255 bytes total − 35-byte fixed header = 220 bytes available for payload)
    uint8_t     payload[220];     // variable-length payload
};

// Maximum wire length: sizeof(MeshPacket)
static_assert(sizeof(MeshPacket) <= 255, "MeshPacket too large for LoRa");
