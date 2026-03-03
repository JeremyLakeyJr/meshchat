// ============================================================
// Chat Manager Implementation
// ============================================================

#include "chat_manager.h"
#include "../mesh/mesh_router.h"
#include "../lora/lora_handler.h"
#include "../bluetooth/ble_handler.h"

ChatManager chatManager;

void ChatManager::begin(const char* nodeName) {
    strncpy(_nodeName, nodeName, sizeof(_nodeName) - 1);

    // Default channel 0: unencrypted "General"
    strncpy(_channels[0].name, "General", CHANNEL_NAME_LEN - 1);
    _channels[0].index     = 0;
    _channels[0].encrypted = false;

    Serial.printf("[Chat] Node name: \"%s\"\n", _nodeName);
}

void ChatManager::sendMessage(const char* text, uint8_t channel) {
    size_t textLen = strlen(text);
    if (textLen == 0 || textLen > MAX_MSG_LEN) return;

    // Build the packet payload: "<name>: <text>\0"
    char payload[sizeof(MeshPacket::payload)];
    int payloadLen = snprintf(payload, sizeof(payload), "%s: %s",
                              _nodeName, text);
    if (payloadLen < 0) return;

    MeshPacket pkt{};
    meshRouter.buildBroadcast(pkt, PacketType::TEXT_MESSAGE,
                               (const uint8_t*)payload, (uint8_t)payloadLen,
                               channel);

    // Transmit over LoRa
    loraHandler.sendPacket(pkt);

    // Also push to BLE peers
    bleHandler.sendPacket(pkt);

    // Add to local history
    ChatMessage msg{};
    strncpy(msg.fromName, _nodeName, sizeof(msg.fromName) - 1);
    memcpy(msg.fromID, meshRouter.myNodeID(), NODE_ID_LEN);
    strncpy(msg.text, text, MAX_MSG_LEN);
    msg.timestampMs = millis();
    msg.channel     = channel;
    msg.encrypted   = _channels[channel].encrypted;

    if (_messages.size() >= MAX_MESSAGES) {
        _messages.erase(_messages.begin());
    }
    _messages.push_back(msg);
}

void ChatManager::sendPrivate(const char* text, const uint8_t dstID[NODE_ID_LEN]) {
    size_t textLen = strlen(text);
    if (textLen == 0 || textLen > MAX_MSG_LEN) return;

    char payload[sizeof(MeshPacket::payload)];
    int payloadLen = snprintf(payload, sizeof(payload), "%s: %s",
                              _nodeName, text);
    if (payloadLen < 0) return;

    MeshPacket pkt{};
    meshRouter.buildUnicast(pkt, dstID, PacketType::PRIVATE_MSG,
                             (const uint8_t*)payload, (uint8_t)payloadLen);
    pkt.flags = pkt.flags | TransportFlags::ENCRYPTED;

    loraHandler.sendPacket(pkt);
}

void ChatManager::handleIncomingPacket(const MeshPacket& pkt,
                                        TransportFlags transport,
                                        int16_t rssi) {
    if (pkt.type != PacketType::TEXT_MESSAGE &&
        pkt.type != PacketType::PRIVATE_MSG) {
        return;
    }

    ChatMessage msg{};
    // Extract name and text from payload "Name: text"
    char raw[221] = {};
    size_t copyLen = pkt.payload_len < sizeof(raw) - 1
                     ? pkt.payload_len : sizeof(raw) - 1;
    memcpy(raw, pkt.payload, copyLen);
    raw[copyLen] = '\0';

    char* sep = strchr(raw, ':');
    if (sep && sep != raw) {
        size_t nameLen = (size_t)(sep - raw);
        if (nameLen >= sizeof(msg.fromName)) nameLen = sizeof(msg.fromName) - 1;
        strncpy(msg.fromName, raw, nameLen);
        msg.fromName[nameLen] = '\0';
        // Skip ": " (colon + optional space) safely
        const char* textStart = sep + 1;
        if (*textStart == ' ') textStart++;
        strncpy(msg.text, textStart, MAX_MSG_LEN);
    } else {
        strncpy(msg.fromName, "?", sizeof(msg.fromName) - 1);
        strncpy(msg.text, raw, MAX_MSG_LEN);
    }

    memcpy(msg.fromID, pkt.from, NODE_ID_LEN);
    msg.timestampMs = millis();
    msg.channel     = pkt.channel;
    msg.encrypted   = (transport & TransportFlags::ENCRYPTED);
    msg.fromBLE     = (transport & TransportFlags::VIA_BLE);
    msg.fromLoRa    = (transport & TransportFlags::VIA_LORA);

    if (_messages.size() >= MAX_MESSAGES) {
        _messages.erase(_messages.begin());
    }
    _messages.push_back(msg);

    uint8_t ch = pkt.channel < MAX_CHANNELS ? pkt.channel : 0;
    _unread[ch]++;

    Serial.printf("[Chat] <%s> %s  (RSSI=%d  %s)\n",
                  msg.fromName, msg.text, rssi,
                  msg.fromLoRa ? "LoRa" : "BLE");
}

uint8_t ChatManager::getDisplayLines(const char** buf, uint8_t maxLines) const {
    // Use a static rotating buffer of formatted strings
    static char lineStore[MAX_MESSAGES][MAX_MSG_LEN + 40];
    static uint8_t lineIdx = 0;

    int count = (int)_messages.size();
    int start = count > maxLines ? count - maxLines : 0;
    uint8_t filled = 0;

    for (int i = start; i < count; i++) {
        uint8_t slot = lineIdx % MAX_MESSAGES;
        formatLine(_messages[i], lineStore[slot], sizeof(lineStore[slot]));
        buf[filled++] = lineStore[slot];
        lineIdx++;
    }
    return filled;
}

uint8_t ChatManager::unreadCount(uint8_t channel) const {
    if (channel >= MAX_CHANNELS) return 0;
    return _unread[channel];
}

void ChatManager::markRead(uint8_t channel) {
    if (channel < MAX_CHANNELS) _unread[channel] = 0;
}

void ChatManager::setChannel(uint8_t index, const char* name,
                              const uint8_t psk[16]) {
    if (index >= MAX_CHANNELS) return;
    _channels[index].index = index;
    strncpy(_channels[index].name, name, CHANNEL_NAME_LEN - 1);
    if (psk) {
        memcpy(_channels[index].psk, psk, 16);
        _channels[index].encrypted = true;
    } else {
        memset(_channels[index].psk, 0, 16);
        _channels[index].encrypted = false;
    }
}

void ChatManager::formatLine(const ChatMessage& msg, char* out, size_t outLen) const {
    uint32_t sec   = msg.timestampMs / 1000;
    uint32_t mins  = (sec / 60) % 60;
    uint32_t hours = (sec / 3600) % 24;
    snprintf(out, outLen, "[%02lu:%02lu] <%s> %s",
             (unsigned long)hours, (unsigned long)mins,
             msg.fromName, msg.text);
}
