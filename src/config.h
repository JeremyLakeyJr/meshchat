#pragma once

// ============================================================
// MeshChat - Hardware & Protocol Configuration
// Supports: Heltec WiFi LoRa 32 V2/V3 and LILYGO T-Deck
// ============================================================

#include <Arduino.h>

// ----------------------------------------------------------
// Firmware mode selection  (runtime, stored in NVS)
//
//   FirmwareMode::BitChat     – BLE uses the Nordic UART Service
//     (NUS) as used by BitChat; LoRa uses MeshChat parameters
//     (SF10, BW125 kHz, sync 0x34, 14 dBm).
//
//   FirmwareMode::Meshtastic  – LoRa parameters and BLE GATT API
//     are fully compatible with the Meshtastic mesh network
//     (sync word 0x2B, LongFast SF11/BW250, Meshtastic GATT).
//
// The active mode is persisted in NVS (Preferences library) and
// can be changed at runtime via the `!mode`, `!bitchat`, or
// `!meshtastic` commands on the serial console or T-Deck keyboard.
// ----------------------------------------------------------
enum class FirmwareMode : uint8_t {
    BitChat    = 1,
    Meshtastic = 2,
};

#define MESHCHAT_NVS_NAMESPACE  "meshchat"
#define MESHCHAT_NVS_MODE_KEY   "mode"
#define MESHCHAT_DEFAULT_MODE   FirmwareMode::BitChat

inline const char* modeName(FirmwareMode m) {
    return (m == FirmwareMode::Meshtastic) ? "Meshtastic" : "BitChat";
}

// Global active mode – defined in main.cpp, declared here so all
// translation units can read it without pulling in Preferences.h.
extern FirmwareMode currentFirmwareMode;

// ----------------------------------------------------------
// Device identity (can be overridden per-board in platformio.ini)
// ----------------------------------------------------------
#ifndef DEVICE_NAME
  #define DEVICE_NAME "MeshChat"
#endif

// ----------------------------------------------------------
// LoRa radio defaults
//
// Both sets of parameters are compiled in; the active mode
// (currentFirmwareMode) selects which values are used at runtime.
//
// Meshtastic LongFast channel preset:
//   SF11, BW250 kHz, CR 4/5, sync word 0x2B, 17 dBm TX.
//   Reference: https://meshtastic.org/docs/overview/radio-settings
//
// BitChat MeshChat custom channel:
//   SF10, BW125 kHz, CR 4/5, sync word 0x34, 14 dBm TX.
// ----------------------------------------------------------
#ifndef LORA_FREQ
  #define LORA_FREQ   915E6    // 915 MHz (US); use 868E6 for EU
#endif

#define LORA_CODING_RATE  5   // 4/5 coding rate (both modes)

// BitChat MeshChat parameters
#define LORA_SF_BITCHAT        10
#define LORA_BW_BITCHAT        125E3
#define LORA_SYNCWORD_BITCHAT  0x34
#define LORA_POWER_BITCHAT     14

// Meshtastic LongFast parameters
#define LORA_SF_MESHTASTIC        11
#define LORA_BW_MESHTASTIC        250E3
#define LORA_SYNCWORD_MESHTASTIC  0x2B
#define LORA_POWER_MESHTASTIC     17

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
// Both sets of UUIDs are compiled in; the active mode selects
// which service is created and advertised at runtime.
//
// Meshtastic BLE GATT service:
//   Service  : 6BA1B218-15A8-461F-9FA8-5D651DEF2B57
//   ToRadio  : F75C76D2-129E-4DAD-A1DD-7866124401E7  (write)
//   FromRadio: 2C55E69E-4993-11ED-B878-0242AC120002  (notify)
//   FromNum  : ED9DA18C-A800-4F66-A670-AA7547ED661A  (notify)
//   Reference: https://github.com/meshtastic/Meshtastic-device
//
// BitChat – Nordic UART Service (NUS):
//   Service  : 6E400001-B5A3-F393-E0A9-E50E24DCCA9E
//   RX (write): 6E400002-B5A3-F393-E0A9-E50E24DCCA9E
//   TX (notify): 6E400003-B5A3-F393-E0A9-E50E24DCCA9E
//   Reference: https://github.com/bitchat
// ----------------------------------------------------------

// Meshtastic GATT UUIDs
#define BLE_MESHTASTIC_SERVICE_UUID   "6BA1B218-15A8-461F-9FA8-5D651DEF2B57"
#define BLE_MESHTASTIC_RX_CHAR_UUID   "F75C76D2-129E-4DAD-A1DD-7866124401E7"
#define BLE_MESHTASTIC_TX_CHAR_UUID   "2C55E69E-4993-11ED-B878-0242AC120002"
#define BLE_MESHTASTIC_FROMNUM_UUID   "ED9DA18C-A800-4F66-A670-AA7547ED661A"

// BitChat / Nordic UART Service UUIDs
#define BLE_BITCHAT_SERVICE_UUID  "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define BLE_BITCHAT_RX_CHAR_UUID  "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define BLE_BITCHAT_TX_CHAR_UUID  "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

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
