// ============================================================
// Mesh Router Implementation
// ============================================================

#include "mesh_router.h"
#include <algorithm>
#include "../lora/lora_handler.h"

MeshRouter meshRouter;

// Broadcast destination (all 0xFF bytes)
static const uint8_t BROADCAST_ADDR[NODE_ID_LEN] = {0xFF, 0xFF, 0xFF, 0xFF};

void MeshRouter::setNodeID(const uint8_t id[NODE_ID_LEN]) {
    memcpy(_myID, id, NODE_ID_LEN);
}

bool MeshRouter::processPacket(const MeshPacket& pkt, TransportFlags transport) {
    // Drop duplicates
    if (isDuplicate(pkt)) return false;
    markSeen(pkt);

    bool forUs = isForUs(pkt) || isBroadcast(pkt);

    // Relay if we are not the destination and hops remain
    if (!isForUs(pkt) && pkt.hop_limit > 0) {
        loraHandler.relayPacket(pkt);
    }

    return forUs;
}

void MeshRouter::buildBroadcast(MeshPacket& pkt, PacketType type,
                                const uint8_t* payload, uint8_t payloadLen,
                                uint8_t channel) {
    memset(&pkt, 0, sizeof(pkt));
    pkt.version    = MESHCHAT_VERSION;
    pkt.type       = type;
    pkt.flags      = TransportFlags::VIA_LORA;
    pkt.packet_id  = ++_txSeq;
    pkt.hop_limit  = MAX_HOPS;
    pkt.hop_start  = MAX_HOPS;
    pkt.channel    = channel;
    memcpy(pkt.from, _myID, NODE_ID_LEN);
    memcpy(pkt.to, BROADCAST_ADDR, NODE_ID_LEN);
    pkt.payload_len = payloadLen;
    if (payloadLen > 0 && payload != nullptr) {
        memcpy(pkt.payload, payload, payloadLen);
    }
}

void MeshRouter::buildUnicast(MeshPacket& pkt, const uint8_t dst[NODE_ID_LEN],
                               PacketType type,
                               const uint8_t* payload, uint8_t payloadLen,
                               uint8_t channel) {
    buildBroadcast(pkt, type, payload, payloadLen, channel);
    pkt.flags = TransportFlags::VIA_LORA | TransportFlags::WANTS_ACK;
    memcpy(pkt.to, dst, NODE_ID_LEN);
}

void MeshRouter::updateNodeRecord(const MeshPacket& pkt,
                                   int16_t rssi, float snr, uint8_t hops) {
    // Find existing record or insert a new one
    for (auto& node : _nodes) {
        if (memcmp(node.id, pkt.from, NODE_ID_LEN) == 0) {
            node.rssi       = rssi;
            node.snr        = snr;
            node.hops       = hops;
            node.lastSeenMs = millis();
            return;
        }
    }
    NodeRecord rec{};
    memcpy(rec.id, pkt.from, NODE_ID_LEN);
    rec.rssi       = rssi;
    rec.snr        = snr;
    rec.hops       = hops;
    rec.lastSeenMs = millis();
    _nodes.push_back(rec);
    Serial.printf("[Mesh] New node: %02X%02X%02X%02X  RSSI=%d  hops=%u\n",
                  rec.id[0], rec.id[1], rec.id[2], rec.id[3], rssi, hops);
}

void MeshRouter::pruneStaleNodes() {
    uint32_t now = millis();
    _nodes.erase(
        std::remove_if(_nodes.begin(), _nodes.end(),
            [now](const NodeRecord& n) {
                return (now - n.lastSeenMs) > NODE_TIMEOUT_MS;
            }),
        _nodes.end());
}

// -------------------------------------------------------------------
// Private helpers
// -------------------------------------------------------------------

static uint32_t computePacketKey(const MeshPacket& pkt) {
    return (((uint32_t)pkt.from[0] << 24 |
             (uint32_t)pkt.from[1] << 16 |
             (uint32_t)pkt.from[2] <<  8 |
             (uint32_t)pkt.from[3])
            ^ ((uint32_t)pkt.packet_id << 16));
}

bool MeshRouter::isDuplicate(const MeshPacket& pkt) {
    uint32_t key = computePacketKey(pkt);
    for (int i = 0; i < PACKET_HISTORY_SIZE; i++) {
        if (_seenPackets[i] == key) return true;
    }
    return false;
}

void MeshRouter::markSeen(const MeshPacket& pkt) {
    _seenPackets[_seenHead] = computePacketKey(pkt);
    _seenHead = (_seenHead + 1) % PACKET_HISTORY_SIZE;
}

bool MeshRouter::isForUs(const MeshPacket& pkt) const {
    return memcmp(pkt.to, _myID, NODE_ID_LEN) == 0;
}

bool MeshRouter::isBroadcast(const MeshPacket& pkt) const {
    return memcmp(pkt.to, BROADCAST_ADDR, NODE_ID_LEN) == 0;
}
