// ============================================================
// WiFi Handler Implementation
// ============================================================

#include "wifi_handler.h"
#include <ArduinoJson.h>
#include <ArduinoOTA.h>

WiFiHandler wifiHandler;

bool WiFiHandler::beginAP(const char* ssid, const char* password) {
    WiFi.mode(WIFI_AP);
    bool ok = password
        ? WiFi.softAP(ssid, password)
        : WiFi.softAP(ssid);
    if (ok) {
        _mode = WiFiMode_t::AP;
        Serial.printf("[WiFi] AP started: SSID=%s  IP=%s\n",
                      ssid, WiFi.softAPIP().toString().c_str());
    } else {
        Serial.println("[WiFi] AP start FAILED");
    }
    return ok;
}

bool WiFiHandler::beginSTA(const char* ssid, const char* password) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    uint32_t start = millis();
    Serial.print("[WiFi] Connecting to ");
    Serial.print(ssid);
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - start > WIFI_CONNECT_TIMEOUT_MS) {
            Serial.println(" TIMEOUT");
            return false;
        }
        delay(250);
        Serial.print('.');
    }
    Serial.printf(" connected  IP=%s\n", WiFi.localIP().toString().c_str());
    _mode = WiFiMode_t::STA;
    return true;
}

bool WiFiHandler::isConnected() const {
    return WiFi.status() == WL_CONNECTED;
}

String WiFiHandler::ipAddress() const {
    if (_mode == WiFiMode_t::AP) return WiFi.softAPIP().toString();
    return WiFi.localIP().toString();
}

bool WiFiHandler::connectMQTT(const char* host, uint16_t port,
                               const char* user, const char* pass) {
    strncpy(_mqttHost, host, sizeof(_mqttHost) - 1);
    _mqttPort = port;
    if (user) strncpy(_mqttUser, user, sizeof(_mqttUser) - 1);
    if (pass) strncpy(_mqttPass, pass, sizeof(_mqttPass) - 1);
    reconnectMQTT();
    return _mqttConnected;
}

bool WiFiHandler::publishPacket(const MeshPacket& pkt) {
    if (!_mqttConnected) return false;

    // Encode packet as JSON for MQTT bridge
    JsonDocument doc;
    char fromStr[9], toStr[9];
    snprintf(fromStr, sizeof(fromStr), "%02X%02X%02X%02X",
             pkt.from[0], pkt.from[1], pkt.from[2], pkt.from[3]);
    snprintf(toStr, sizeof(toStr), "%02X%02X%02X%02X",
             pkt.to[0], pkt.to[1], pkt.to[2], pkt.to[3]);

    doc["ver"]  = pkt.version;
    doc["type"] = static_cast<uint8_t>(pkt.type);
    doc["from"] = fromStr;
    doc["to"]   = toStr;
    doc["id"]   = pkt.packet_id;
    doc["hops"] = pkt.hop_limit;
    doc["ch"]   = pkt.channel;

    // Embed text payload when type is TEXT_MESSAGE
    if (pkt.type == PacketType::TEXT_MESSAGE && pkt.payload_len > 0) {
        char msg[221];
        memcpy(msg, pkt.payload, pkt.payload_len);
        msg[pkt.payload_len] = '\0';
        doc["msg"] = msg;
    }

    char json[512];
    size_t jsonLen = serializeJson(doc, json, sizeof(json));

    // MQTT publish is handled by the caller via a higher-level MQTT client.
    // Here we just log it so the WiFi handler remains library-agnostic.
    Serial.printf("[WiFi/MQTT] %s -> %s\n", MQTT_TOPIC_ROOT, json);
    (void)jsonLen;
    return true;
}

bool WiFiHandler::receiveMQTT(MeshPacket& pkt) {
    // MQTT receive handled via callback in a higher-level MQTT client.
    // This stub returns false; integrate PubSubClient or similar as needed.
    (void)pkt;
    return false;
}

void WiFiHandler::startOTA(const char* hostname) {
    ArduinoOTA.setHostname(hostname);
    ArduinoOTA.onStart([]() {
        Serial.println("[OTA] Starting firmware update...");
    });
    ArduinoOTA.onEnd([]() {
        Serial.println("[OTA] Update complete, rebooting.");
    });
    ArduinoOTA.onError([](ota_error_t err) {
        Serial.printf("[OTA] Error[%u]\n", err);
    });
    ArduinoOTA.begin();
    Serial.printf("[OTA] Ready  hostname=%s\n", hostname);
}

void WiFiHandler::update() {
    if (_mode == WiFiMode_t::STA || _mode == WiFiMode_t::AP_STA) {
        ArduinoOTA.handle();
    }
    // MQTT keepalive / reconnect (every 5 s)
    if (_mqttConnected) return;
    if (millis() - _lastMqttReconnectMs > 5000) {
        _lastMqttReconnectMs = millis();
        if (strlen(_mqttHost) > 0) reconnectMQTT();
    }
}

void WiFiHandler::reconnectMQTT() {
    // Reconnection logic is intentionally a stub.
    // Integrate with PubSubClient or similar library as needed.
    Serial.printf("[WiFi] MQTT broker: %s:%u (integrate PubSubClient to enable)\n",
                  _mqttHost, _mqttPort);
}
