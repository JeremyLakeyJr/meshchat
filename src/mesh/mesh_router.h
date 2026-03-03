#pragma once

// ============================================================
// Mesh Router - flood-and-forward routing across LoRa + BLE
// Maintains a routing table, deduplicates packets, manages ACKs
// ============================================================

#include <Arduino.h>
#include <map>
#include <vector>
#include "../config.h"

struct NodeRecord {
    uint8_t  id[NODE_ID_LEN];
    char     name[32];
    int16_t  rssi;
    float    snr;
    uint8_t  hops;            // hops from us (0 = direct)
    uint32_t lastSeenMs;
    float    latitude;
    float    longitude;
    int32_t  altitudeM;
    uint8_t  batteryPct;
};

class MeshRouter {
public:
    // Set our own node ID (derived from ESP32 MAC).
    void setNodeID(const uint8_t id[NODE_ID_LEN]);

    // Process an incoming packet (from any transport).
    // Returns true if this node is the final destination.
    // Automatically relays broadcast/transit packets.
    bool processPacket(const MeshPacket& pkt, TransportFlags transport);

    // Build a new outgoing broadcast packet.
    void buildBroadcast(MeshPacket& pkt, PacketType type,
                        const uint8_t* payload, uint8_t payloadLen,
                        uint8_t channel = 0);

    // Build a new outgoing unicast packet.
    void buildUnicast(MeshPacket& pkt, const uint8_t dst[NODE_ID_LEN],
                      PacketType type,
                      const uint8_t* payload, uint8_t payloadLen,
                      uint8_t channel = 0);

    // Returns the list of currently known nodes.
    const std::vector<NodeRecord>& nodes() const { return _nodes; }

    // Update a node record from a received HEARTBEAT or NODE_INFO packet.
    void updateNodeRecord(const MeshPacket& pkt, int16_t rssi, float snr, uint8_t hops);

    // Expire stale nodes.
    void pruneStaleNodes();

    // Returns our own node ID.
    const uint8_t* myNodeID() const { return _myID; }

private:
    uint8_t _myID[NODE_ID_LEN] = {};
    uint16_t _txSeq = 0;

    // Seen-packet deduplication ring buffer: key = (from[0..3] << 16 | packet_id)
    uint32_t _seenPackets[PACKET_HISTORY_SIZE] = {};
    uint8_t  _seenHead = 0;

    std::vector<NodeRecord> _nodes;

    bool isDuplicate(const MeshPacket& pkt);
    void markSeen(const MeshPacket& pkt);
    bool isForUs(const MeshPacket& pkt) const;
    bool isBroadcast(const MeshPacket& pkt) const;
};

extern MeshRouter meshRouter;
