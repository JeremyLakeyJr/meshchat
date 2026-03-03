#pragma once

// ============================================================
// MeshChat - Hardware & Protocol Configuration
// Supports: Heltec WiFi LoRa 32 V2/V3 and LILYGO T-Deck
// ============================================================

#include <Arduino.h>

// ----------------------------------------------------------
// Firmware mode selection
//
//   FIRMWARE_MODE_MESHTASTIC  – LoRa parameters and BLE API are
//     fully compatible with the Meshtastic open-source mesh
//     network (sync word 0x2B, LongFast SF11/BW250, Meshtastic
//     BLE GATT service).
//
//   FIRMWARE_MODE_BITCHAT     – BLE-primary operation using the
//     Nordic UART Service (NUS) UUIDs used by BitChat, with
//     standard LoRa parameters for the MeshChat custom mesh.
//
// Select a mode by setting -DFIRMWARE_MODE=<value> in
// platformio.ini build_flags, or it defaults to BitChat.
// ----------------------------------------------------------
#define FIRMWARE_MODE_MESHTASTIC  1
#define FIRMWARE_MODE_BITCHAT     2

#ifndef FIRMWARE_MODE
  #define FIRMWARE_MODE  FIRMWARE_MODE_BITCHAT
#endif

#if FIRMWARE_MODE == FIRMWARE_MODE_MESHTASTIC
  #define FIRMWARE_MODE_NAME  "Meshtastic"
#else
  #define FIRMWARE_MODE_NAME  "BitChat"
#endif

// ----------------------------------------------------------
// Device identity (can be overridden per-board in platformio.ini)
// ----------------------------------------------------------
#ifndef DEVICE_NAME
  #define DEVICE_NAME "MeshChat"
#endif

// ----------------------------------------------------------
// LoRa radio defaults
//
// Meshtastic mode uses the Meshtastic LongFast channel preset:
//   SF11, BW250 kHz, CR 4/5, sync word 0x2B, 17 dBm TX.
//   Reference: https://meshtastic.org/docs/overview/radio-settings
//
// BitChat mode uses the MeshChat custom channel:
//   SF10, BW125 kHz, CR 4/5, sync word 0x34, 14 dBm TX.
//
// All values can be overridden individually in platformio.ini.
// ----------------------------------------------------------
#ifndef LORA_FREQ
  #define LORA_FREQ   915E6    // 915 MHz (US); use 868E6 for EU
#endif

#ifndef LORA_BANDWIDTH
  #if FIRMWARE_MODE == FIRMWARE_MODE_MESHTASTIC
    #define LORA_BANDWIDTH  250E3   // Meshtastic LongFast: 250 kHz
  #else
    #define LORA_BANDWIDTH  125E3   // BitChat MeshChat: 125 kHz
  #endif
#endif

#ifndef LORA_SPREADING_FACTOR
  #if FIRMWARE_MODE == FIRMWARE_MODE_MESHTASTIC
    #define LORA_SPREADING_FACTOR  11   // Meshtastic LongFast: SF11
  #else
    #define LORA_SPREADING_FACTOR  10   // BitChat MeshChat: SF10
  #endif
#endif

#ifndef LORA_CODING_RATE
  #define LORA_CODING_RATE  5   // 4/5 coding rate (both modes)
#endif

#ifndef LORA_SYNC_WORD
  #if FIRMWARE_MODE == FIRMWARE_MODE_MESHTASTIC
    #define LORA_SYNC_WORD  0x2B   // Meshtastic private network identifier
  #else
    #define LORA_SYNC_WORD  0x34   // BitChat MeshChat network identifier
  #endif
#endif

#ifndef LORA_TX_POWER
  #if FIRMWARE_MODE == FIRMWARE_MODE_MESHTASTIC
    #define LORA_TX_POWER   17     // dBm – Meshtastic default
  #else
    #define LORA_TX_POWER   14     // dBm – BitChat MeshChat default
  #endif
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
// BLE service / characteristic UUIDs
//
// Meshtastic mode uses the official Meshtastic BLE GATT service:
//   Service  : 6BA1B218-15A8-461F-9FA8-5D651DEF2B57
//   ToRadio  : F75C76D2-129E-4DAD-A1DD-7866124401E7  (write)
//   FromRadio: 2C55E69E-4993-11ED-B878-0242AC120002  (notify)
//   FromNum  : ED9DA18C-A800-4F66-A670-AA7547ED661A  (notify)
//   Reference: https://github.com/meshtastic/Meshtastic-device
//
// BitChat mode uses the Nordic UART Service (NUS):
//   Service  : 6E400001-B5A3-F393-E0A9-E50E24DCCA9E
//   RX (write): 6E400002-B5A3-F393-E0A9-E50E24DCCA9E
//   TX (notify): 6E400003-B5A3-F393-E0A9-E50E24DCCA9E
//   Reference: https://github.com/bitchat
// ----------------------------------------------------------
#if FIRMWARE_MODE == FIRMWARE_MODE_MESHTASTIC
  #define BLE_SERVICE_UUID       "6BA1B218-15A8-461F-9FA8-5D651DEF2B57"
  #define BLE_RX_CHAR_UUID       "F75C76D2-129E-4DAD-A1DD-7866124401E7"  // ToRadio
  #define BLE_TX_CHAR_UUID       "2C55E69E-4993-11ED-B878-0242AC120002"  // FromRadio
  #define BLE_FROMNUM_CHAR_UUID  "ED9DA18C-A800-4F66-A670-AA7547ED661A"  // FromNum
#else
  // BitChat – Nordic UART Service
  #define BLE_SERVICE_UUID       "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
  #define BLE_RX_CHAR_UUID       "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
  #define BLE_TX_CHAR_UUID       "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"
#endif

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
