#pragma once

// ============================================================
// Chat Manager - unified messaging layer
// Stores message history, handles channel keys, routes
// outgoing messages to the correct transport (LoRa / BLE / WiFi)
// ============================================================

#include <Arduino.h>
#include <vector>
#include "../config.h"

#define MAX_MESSAGES     64    // rolling history depth
#define MAX_MSG_LEN      200   // characters per message
#define MAX_CHANNELS     8     // logical channels supported
#define CHANNEL_NAME_LEN 20

struct ChatMessage {
    char     fromName[32];
    uint8_t  fromID[NODE_ID_LEN];
    char     text[MAX_MSG_LEN + 1];
    uint32_t timestampMs;
    uint8_t  channel;
    bool     encrypted;
    bool     fromBLE;   // true = arrived via BLE transport
    bool     fromLoRa;  // true = arrived via LoRa transport
};

struct Channel {
    uint8_t  index;
    char     name[CHANNEL_NAME_LEN];
    uint8_t  psk[16];      // pre-shared key (AES-128)
    bool     encrypted;
};

class ChatManager {
public:
    // Initialise with a user-visible node name.
    void begin(const char* nodeName);

    // Send a text message on the given channel.
    // Dispatches over LoRa by default; BLE peers also receive it.
    void sendMessage(const char* text, uint8_t channel = 0);

    // Send a private (encrypted) message to a specific node.
    void sendPrivate(const char* text, const uint8_t dstID[NODE_ID_LEN]);

    // Process an incoming MeshPacket and add it to history if it's a chat message.
    void handleIncomingPacket(const MeshPacket& pkt,
                               TransportFlags transport,
                               int16_t rssi = 0);

    // Returns the message history (newest last).
    const std::vector<ChatMessage>& messages() const { return _messages; }

    // Returns the message history as a flat array of C-strings for the display.
    // Fills buf with up to maxLines pointers into internal storage.
    uint8_t getDisplayLines(const char** buf, uint8_t maxLines) const;

    // Returns the number of unread messages on the given channel.
    uint8_t unreadCount(uint8_t channel = 0) const;

    // Mark all messages on a channel as read.
    void markRead(uint8_t channel = 0);

    // Add or update a channel definition.
    void setChannel(uint8_t index, const char* name,
                    const uint8_t psk[16] = nullptr);

    // Our display name.
    const char* nodeName() const { return _nodeName; }

private:
    char _nodeName[32] = "MeshChat";

    std::vector<ChatMessage> _messages;
    uint8_t _unread[MAX_CHANNELS] = {};
    Channel _channels[MAX_CHANNELS] = {};

    // Format a display line: "[HH:MM] <Name> text"
    void formatLine(const ChatMessage& msg, char* out, size_t outLen) const;
};

extern ChatManager chatManager;
