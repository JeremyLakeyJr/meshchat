// ============================================================
// LoRa Handler Implementation
// ============================================================

#include "lora_handler.h"
#include <SPI.h>
#include <LoRa.h>

LoRaHandler loraHandler;

bool LoRaHandler::begin(int sck, int miso, int mosi, int ss, int rst, int dio0) {
    SPI.begin(sck, miso, mosi, ss);
    LoRa.setPins(ss, rst, dio0);

    if (!LoRa.begin(LORA_FREQ)) {
        Serial.println("[LoRa] Radio init FAILED");
        return false;
    }

    // Select parameters based on the active runtime mode
    int   sf;
    long  bw;
    int   syncWord;
    int   txPower;
    if (currentFirmwareMode == FirmwareMode::Meshtastic) {
        sf       = LORA_SF_MESHTASTIC;
        bw       = (long)LORA_BW_MESHTASTIC;
        syncWord = LORA_SYNCWORD_MESHTASTIC;
        txPower  = LORA_POWER_MESHTASTIC;
    } else {
        sf       = LORA_SF_BITCHAT;
        bw       = (long)LORA_BW_BITCHAT;
        syncWord = LORA_SYNCWORD_BITCHAT;
        txPower  = LORA_POWER_BITCHAT;
    }

    LoRa.setSpreadingFactor(sf);
    LoRa.setSignalBandwidth(bw);
    LoRa.setCodingRate4(LORA_CODING_RATE);
    LoRa.setSyncWord(syncWord);
    LoRa.setTxPower(txPower);
    LoRa.enableCrc();

    Serial.printf("[LoRa] Mode: %s  %.3f MHz  SF%d  BW%.0fkHz  SyncWord=0x%02X\n",
                  modeName(currentFirmwareMode),
                  (double)LORA_FREQ / 1e6,
                  sf,
                  (double)bw / 1e3,
                  syncWord);
    return true;
}

bool LoRaHandler::sendPacket(const MeshPacket& pkt) {
    if (_txBusy) return false;
    _txBusy = true;

    uint8_t buf[sizeof(MeshPacket)];
    size_t len = encodePacket(pkt, buf, sizeof(buf));

    LoRa.beginPacket();
    LoRa.write(buf, len);
    LoRa.endPacket(true);   // async=true: non-blocking TX
    _txBusy = false;
    return true;
}

bool LoRaHandler::receivePacket(MeshPacket& pkt) {
    int pktSize = LoRa.parsePacket();
    if (pktSize == 0) return false;

    uint8_t buf[sizeof(MeshPacket)];
    size_t idx = 0;
    while (LoRa.available() && idx < sizeof(buf)) {
        buf[idx++] = (uint8_t)LoRa.read();
    }

    _lastRSSI = LoRa.packetRssi();
    _lastSNR  = LoRa.packetSnr();

    return decodePacket(buf, idx, pkt);
}

void LoRaHandler::update() {
    // Placeholder for any periodic radio maintenance (CAD, duty cycle, etc.)
}

void LoRaHandler::relayPacket(const MeshPacket& pkt) {
    if (pkt.hop_limit == 0) return;   // do not relay exhausted packets

    // Random back-off [0, FLOOD_REBROADCAST_DELAY) ms to reduce collisions.
    // NOTE: This uses a blocking delay which pauses the main loop briefly.
    // For latency-sensitive applications, replace with a non-blocking timer queue.
    delay(random(0, FLOOD_REBROADCAST_DELAY));

    MeshPacket relay = pkt;
    relay.hop_limit--;
    sendPacket(relay);
}

// -------------------------------------------------------------------
// Private helpers
// -------------------------------------------------------------------

size_t LoRaHandler::encodePacket(const MeshPacket& pkt, uint8_t* buf, size_t bufLen) {
    // Wire format: fixed header (10 bytes) + payload
    const size_t headerLen = offsetof(MeshPacket, payload);
    size_t totalLen = headerLen + pkt.payload_len;
    if (totalLen > bufLen) totalLen = bufLen;
    memcpy(buf, &pkt, totalLen);
    return totalLen;
}

bool LoRaHandler::decodePacket(const uint8_t* buf, size_t len, MeshPacket& pkt) {
    const size_t headerLen = offsetof(MeshPacket, payload);
    if (len < headerLen) return false;

    memcpy(&pkt, buf, len);

    // Basic validation
    if (pkt.version != MESHCHAT_VERSION) return false;
    if (pkt.payload_len > sizeof(pkt.payload)) return false;
    if (headerLen + pkt.payload_len > len) return false;

    return true;
}
