#pragma once

// ============================================================
// WiFi Handler - provides AP/STA mode, MQTT bridge, OTA
// BitChat uses WiFi when LoRa & BLE peers are out of range
// ============================================================

#include <Arduino.h>
#include <WiFi.h>
#include "../config.h"

enum class WiFiMode_t : uint8_t {
    DISABLED = 0,
    AP,       // Access Point (creates its own network)
    STA,      // Station (joins an existing network)
    AP_STA,   // Both simultaneously
};

class WiFiHandler {
public:
    // Start in Access Point mode (creates "MeshChat-<id>" network).
    bool beginAP(const char* ssid, const char* password = nullptr);

    // Connect to an existing WiFi network.
    bool beginSTA(const char* ssid, const char* password);

    // Returns true when a STA connection is active.
    bool isConnected() const;

    // Returns the current IP address.
    String ipAddress() const;

    // Optional: connect to an MQTT broker to bridge mesh messages.
    bool connectMQTT(const char* host, uint16_t port,
                     const char* user = nullptr,
                     const char* pass = nullptr);

    // Publish a mesh packet to MQTT (JSON-encoded).
    bool publishPacket(const MeshPacket& pkt);

    // Poll for MQTT messages and fill pkt if available.
    bool receiveMQTT(MeshPacket& pkt);

    // Start Arduino OTA for wireless firmware updates.
    void startOTA(const char* hostname);

    // Must be called from the main loop.
    void update();

    // Current operating mode.
    WiFiMode_t mode() const { return _mode; }

private:
    WiFiMode_t _mode = WiFiMode_t::DISABLED;

    // MQTT state
    bool     _mqttConnected = false;
    char     _mqttHost[64]  = {};
    uint16_t _mqttPort      = MQTT_PORT;
    char     _mqttUser[32]  = {};
    char     _mqttPass[32]  = {};
    uint32_t _lastMqttReconnectMs = 0;

    void reconnectMQTT();
};

extern WiFiHandler wifiHandler;
