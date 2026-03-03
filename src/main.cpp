// ============================================================
// MeshChat - Main Firmware Entry Point
//
// Hybrid Meshtastic + BitChat OS for LoRa ESP32 hardware
//   • Heltec WiFi LoRa 32 V2 / V3
//   • LILYGO T-Deck
//
// Architecture:
//   LoRa  ←→  MeshRouter  ←→  ChatManager  ←→  Display
//                  ↕                  ↕
//   BLE  (BitChat-style peer-to-peer Bluetooth LE mesh)
//                  ↕
//   WiFi (AP or STA + optional MQTT bridge + OTA updates)
// ============================================================

#include <Arduino.h>
#include <esp_mac.h>
#include "config.h"
#include "lora/lora_handler.h"
#include "bluetooth/ble_handler.h"
#include "wifi/wifi_handler.h"
#include "display/display_handler.h"
#include "mesh/mesh_router.h"
#include "chat/chat_manager.h"

// -------------------------------------------------------------------
// Timing
// -------------------------------------------------------------------
static uint32_t lastHeartbeatMs   = 0;
static uint32_t lastDisplayRefresh = 0;
static uint32_t lastNodePruneMs   = 0;

// -------------------------------------------------------------------
// Derive a 4-byte node ID from the ESP32's unique MAC address
// -------------------------------------------------------------------
static void deriveNodeID(uint8_t id[NODE_ID_LEN]) {
    uint8_t mac[6];
    // Use esp_read_mac for forward compatibility with newer ESP-IDF versions
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    // XOR fold 6 bytes into 4 for the node ID
    id[0] = mac[0] ^ mac[4];
    id[1] = mac[1] ^ mac[5];
    id[2] = mac[2];
    id[3] = mac[3];
}

// -------------------------------------------------------------------
// Build and broadcast a HEARTBEAT packet
// -------------------------------------------------------------------
static void sendHeartbeat() {
    // Payload: null-terminated device name
    const char* name = chatManager.nodeName();
    MeshPacket pkt{};
    meshRouter.buildBroadcast(pkt, PacketType::HEARTBEAT,
                               (const uint8_t*)name,
                               (uint8_t)strlen(name));
    loraHandler.sendPacket(pkt);
    bleHandler.sendPacket(pkt);
}

// -------------------------------------------------------------------
// Process a packet received from any transport
// -------------------------------------------------------------------
static void handlePacket(const MeshPacket& pkt, TransportFlags transport,
                          int16_t rssi = 0) {
    bool forUs = meshRouter.processPacket(pkt, transport);
    if (!forUs) return;

    switch (pkt.type) {
        case PacketType::TEXT_MESSAGE:
        case PacketType::PRIVATE_MSG:
            chatManager.handleIncomingPacket(pkt, transport, rssi);
            {
                char toast[48];
                snprintf(toast, sizeof(toast), "New msg (RSSI %d)", rssi);
                displayHandler.showToast(toast);
            }
            break;

        case PacketType::HEARTBEAT:
        case PacketType::NODE_INFO:
            meshRouter.updateNodeRecord(pkt, rssi, loraHandler.lastSNR(),
                                         pkt.hop_start - pkt.hop_limit);
            break;

        case PacketType::ACK:
            // ACK handling: log for now; extend with retry queue as needed
            Serial.printf("[Main] ACK for pkt_id=%u\n", pkt.packet_id);
            break;

        default:
            break;
    }
}

// -------------------------------------------------------------------
// Keyboard input (T-Deck only)
// -------------------------------------------------------------------
#ifdef HAS_KEYBOARD
#include <Wire.h>
static String _inputBuffer;

static void pollKeyboard() {
    Wire.requestFrom(0x55, 1);   // T-Deck keyboard I2C address
    while (Wire.available()) {
        char c = (char)Wire.read();
        if (c == '\r' || c == '\n') {
            if (_inputBuffer.length() > 0) {
                chatManager.sendMessage(_inputBuffer.c_str());
                _inputBuffer = "";
            }
        } else if (c == 0x08 || c == 0x7F) {  // backspace
            if (_inputBuffer.length() > 0) {
                _inputBuffer.remove(_inputBuffer.length() - 1);
            }
        } else if (c >= 0x20) {
            _inputBuffer += c;
        }
    }
}
#endif

// -------------------------------------------------------------------
// setup()
// -------------------------------------------------------------------
void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n========================================");
    Serial.println(" MeshChat - LoRa ESP32 Hybrid OS");
    Serial.printf(" Board: %s\n",
#if defined(BOARD_HELTEC_V3)
        "Heltec WiFi LoRa 32 V3"
#elif defined(BOARD_HELTEC_V2)
        "Heltec WiFi LoRa 32 V2"
#elif defined(BOARD_TDECK)
        "LILYGO T-Deck"
#else
        "Unknown"
#endif
    );
    Serial.println("========================================\n");

    // Derive node identity from ESP32 MAC
    uint8_t nodeID[NODE_ID_LEN];
    deriveNodeID(nodeID);
    meshRouter.setNodeID(nodeID);
    Serial.printf("[Main] Node ID: %02X%02X%02X%02X\n",
                  nodeID[0], nodeID[1], nodeID[2], nodeID[3]);

    // Build a device name: DEVICE_NAME + last 2 bytes of node ID
    char name[48];
    snprintf(name, sizeof(name), "%s-%02X%02X", DEVICE_NAME,
             nodeID[2], nodeID[3]);

    // ── Display ────────────────────────────────────────────────────
    displayHandler.begin();

    // ── ChatManager ────────────────────────────────────────────────
    chatManager.begin(name);

    // ── LoRa radio ─────────────────────────────────────────────────
    bool loraOk = loraHandler.begin(
        LORA_SCK, LORA_MISO, LORA_MOSI,
        LORA_SS, LORA_RST, LORA_DIO0);
    if (!loraOk) {
        displayHandler.showToast("LoRa FAIL!", 5000);
    }

    // ── Bluetooth LE ───────────────────────────────────────────────
    bool bleOk = bleHandler.begin(name);
    if (!bleOk) {
        displayHandler.showToast("BLE FAIL!", 3000);
    }

    // ── WiFi: start as AP so users can configure a phone/laptop ────
    char apSSID[48];
    snprintf(apSSID, sizeof(apSSID), "MeshChat-%02X%02X",
             nodeID[2], nodeID[3]);
    wifiHandler.beginAP(apSSID, "meshchat");
    wifiHandler.startOTA(name);

#ifdef HAS_KEYBOARD
    Wire.begin(KEYBOARD_SDA, KEYBOARD_SCL);
#endif

    // Send initial heartbeat
    sendHeartbeat();
    lastHeartbeatMs = millis();

    displayHandler.drawStatusBar(loraOk, bleOk, true,
                                  loraHandler.lastRSSI(), 100);
    displayHandler.showToast("MeshChat ready!", 2000);
    Serial.println("[Main] Setup complete\n");
}

// -------------------------------------------------------------------
// loop()
// -------------------------------------------------------------------
void loop() {
    uint32_t now = millis();

    // ── Poll LoRa RX ───────────────────────────────────────────────
    loraHandler.update();
    {
        MeshPacket pkt{};
        if (loraHandler.receivePacket(pkt)) {
            handlePacket(pkt, TransportFlags::VIA_LORA,
                         loraHandler.lastRSSI());
        }
    }

    // ── Poll BLE RX ────────────────────────────────────────────────
    bleHandler.update();
    {
        MeshPacket pkt{};
        if (bleHandler.receivePacket(pkt)) {
            handlePacket(pkt, TransportFlags::VIA_BLE);
        }
    }

    // ── Poll WiFi / OTA ────────────────────────────────────────────
    wifiHandler.update();

    // ── Keyboard input (T-Deck) ────────────────────────────────────
#ifdef HAS_KEYBOARD
    pollKeyboard();
#endif

    // ── Serial console input (debug / any board) ───────────────────
    if (Serial.available()) {
        String line = Serial.readStringUntil('\n');
        line.trim();
        if (line.length() > 0) {
            chatManager.sendMessage(line.c_str());
            Serial.printf("[Serial] Sent: %s\n", line.c_str());
        }
    }

    // ── Periodic heartbeat ─────────────────────────────────────────
    if (now - lastHeartbeatMs >= HEARTBEAT_INTERVAL_MS) {
        lastHeartbeatMs = now;
        sendHeartbeat();
    }

    // ── Periodic node table prune ──────────────────────────────────
    if (now - lastNodePruneMs >= NODE_TIMEOUT_MS / 5) {
        lastNodePruneMs = now;
        meshRouter.pruneStaleNodes();
    }

    // ── Display refresh ────────────────────────────────────────────
    if (now - lastDisplayRefresh >= UI_REFRESH_MS) {
        lastDisplayRefresh = now;

        displayHandler.update();

        // Status bar
        displayHandler.drawStatusBar(
            true,                           // LoRa always on
            bleHandler.connectedPeers() > 0,
            wifiHandler.isConnected(),
            loraHandler.lastRSSI(),
            100 /* battery % placeholder; read ADC for real value */);

        // Chat screen
        if (displayHandler.screen() == 0) {
            const char* lines[16];
            uint8_t lineCount = chatManager.getDisplayLines(lines, 16);
            displayHandler.drawChat(lines, lineCount);
        } else if (displayHandler.screen() == 1) {
            // Node list
            const auto& nodes = meshRouter.nodes();
            static const char* nodeNames[16];
            static int16_t     nodeRSSI[16];
            static uint8_t     nodeHops[16];
            uint8_t n = 0;
            for (const auto& nd : nodes) {
                if (n >= 16) break;
                nodeNames[n] = nd.name[0] ? nd.name : "?";
                nodeRSSI[n]  = nd.rssi;
                nodeHops[n]  = nd.hops;
                n++;
            }
            displayHandler.drawNodeList(nodeNames, nodeRSSI, nodeHops, n);
        }
    }
}
