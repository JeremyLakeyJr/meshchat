#pragma once

// ============================================================
// LoRa Handler - Meshtastic-style mesh over LoRa radio
// Handles: radio init, TX/RX, packet flooding, SNR-based relay
// ============================================================

#include <Arduino.h>
#include "../config.h"

class LoRaHandler {
public:
    // Initialize the LoRa radio with board-specific pins.
    // Returns true on success.
    bool begin(int sck, int miso, int mosi, int ss, int rst, int dio0);

    // Send a mesh packet over LoRa.
    // Returns true if queued successfully.
    bool sendPacket(const MeshPacket& pkt);

    // Poll for an incoming LoRa frame.
    // Fills pkt and returns true when a valid frame is received.
    bool receivePacket(MeshPacket& pkt);

    // Returns the RSSI of the last received packet (dBm).
    int lastRSSI() const { return _lastRSSI; }

    // Returns the SNR of the last received packet (dB × 4).
    float lastSNR() const { return _lastSNR; }

    // Must be called from the main loop to handle async RX.
    void update();

    // Re-broadcast a received packet as a relay (flood mesh).
    // Applies a short random back-off to reduce collision probability.
    void relayPacket(const MeshPacket& pkt);

    // Returns true when the radio is currently transmitting.
    bool isTxBusy() const { return _txBusy; }

private:
    int   _lastRSSI = 0;
    float _lastSNR  = 0.0f;
    bool  _txBusy   = false;

    // Encode a MeshPacket into the raw byte buffer used by the LoRa library.
    size_t encodePacket(const MeshPacket& pkt, uint8_t* buf, size_t bufLen);

    // Decode raw bytes into a MeshPacket.
    bool decodePacket(const uint8_t* buf, size_t len, MeshPacket& pkt);
};

extern LoRaHandler loraHandler;
