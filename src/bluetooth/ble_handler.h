#pragma once

// ============================================================
// BLE Handler - BitChat-style Bluetooth LE mesh chat
// Uses NimBLE-Arduino for lower memory footprint
// Advertises as a peripheral AND scans for other MeshChat nodes
// ============================================================

#include <Arduino.h>
#include <NimBLEDevice.h>
#include "../config.h"

// Forward declarations
class BLECallbacksImpl;

class BLEHandler {
public:
    // Start BLE advertising and scanning.
    // nodeName: human-readable device name broadcast via BLE.
    bool begin(const char* nodeName);

    // Send a mesh packet to all connected BLE peers.
    void sendPacket(const MeshPacket& pkt);

    // Poll for an incoming BLE packet.
    // Fills pkt and returns true when data is available.
    bool receivePacket(MeshPacket& pkt);

    // Called from the main loop.
    void update();

    // Number of currently connected BLE peers.
    size_t connectedPeers() const;

private:
    NimBLEServer*         _server      = nullptr;
    NimBLECharacteristic* _txChar      = nullptr;
    NimBLECharacteristic* _rxChar      = nullptr;
#if FIRMWARE_MODE == FIRMWARE_MODE_MESHTASTIC
    // Meshtastic FromNum characteristic: notified (incremented) whenever a
    // new FromRadio packet is available, so connected clients know to read it.
    NimBLECharacteristic* _fromNumChar = nullptr;
    uint32_t              _fromNum     = 0;
#endif
    NimBLEScan*           _scan        = nullptr;

    // Ring buffer for received packets (ISR-safe)
    static constexpr size_t RX_QUEUE_SIZE = 8;
    MeshPacket  _rxQueue[RX_QUEUE_SIZE];
    volatile uint8_t _rxHead = 0;
    volatile uint8_t _rxTail = 0;

    uint32_t _lastScanMs = 0;

    void startScan();
    void startAdvertising(const char* name);

    friend class BLESrvCallbacks;
    friend class BLECharCallbacks;
};

extern BLEHandler bleHandler;
