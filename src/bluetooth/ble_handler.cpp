// ============================================================
// BLE Handler Implementation (NimBLE-Arduino)
// ============================================================

#include "ble_handler.h"
#include <NimBLEServer.h>
#include <NimBLECharacteristic.h>
#include <NimBLEAdvertising.h>

BLEHandler bleHandler;

// -------------------------------------------------------------------
// Server-level callbacks (connection / disconnection events)
// -------------------------------------------------------------------
class BLESrvCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pServer, ble_gap_conn_desc* desc) override {
        Serial.printf("[BLE] Peer connected: %s\n",
                      NimBLEAddress(desc->peer_ota_addr).toString().c_str());
        // Resume advertising so additional clients can connect
        NimBLEDevice::startAdvertising();
    }
    void onDisconnect(NimBLEServer* pServer) override {
        Serial.println("[BLE] Peer disconnected");
        NimBLEDevice::startAdvertising();
    }
};

// -------------------------------------------------------------------
// Characteristic write callback (incoming BLE → MeshPacket)
// -------------------------------------------------------------------
class BLECharCallbacks : public NimBLECharacteristicCallbacks {
public:
    explicit BLECharCallbacks(BLEHandler* h) : _handler(h) {}

    void onWrite(NimBLECharacteristic* pChar) override {
        std::string data = pChar->getValue();
        if (data.size() < offsetof(MeshPacket, payload)) return;

        MeshPacket pkt{};
        size_t copyLen = std::min(data.size(), sizeof(MeshPacket));
        memcpy(&pkt, data.data(), copyLen);

        if (pkt.version != MESHCHAT_VERSION) return;

        uint8_t nextHead = (_handler->_rxHead + 1) % BLEHandler::RX_QUEUE_SIZE;
        if (nextHead != _handler->_rxTail) {
            _handler->_rxQueue[_handler->_rxHead] = pkt;
            _handler->_rxHead = nextHead;
        } else {
            Serial.println("[BLE] RX queue full — packet dropped");
        }
    }
private:
    BLEHandler* _handler;
};

// -------------------------------------------------------------------
// BLEHandler implementation
// -------------------------------------------------------------------

bool BLEHandler::begin(const char* nodeName) {
    NimBLEDevice::init(nodeName);
    NimBLEDevice::setPower(ESP_PWR_LVL_P9);   // maximum TX power

    // Create GATT server
    _server = NimBLEDevice::createServer();
    _server->setCallbacks(new BLESrvCallbacks());

    // Create service
    NimBLEService* svc = _server->createService(BLE_SERVICE_UUID);

    // TX characteristic: notify-able, server → client
    _txChar = svc->createCharacteristic(
        BLE_TX_CHAR_UUID,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);

    // RX characteristic: writable, client → server
    _rxChar = svc->createCharacteristic(
        BLE_RX_CHAR_UUID,
        NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
    _rxChar->setCallbacks(new BLECharCallbacks(this));

    svc->start();
    startAdvertising(nodeName);
    startScan();

    Serial.printf("[BLE] Advertising as \"%s\"\n", nodeName);
    return true;
}

void BLEHandler::sendPacket(const MeshPacket& pkt) {
    if (!_txChar) return;
    const size_t headerLen = offsetof(MeshPacket, payload);
    size_t totalLen = headerLen + pkt.payload_len;
    if (totalLen > sizeof(MeshPacket)) totalLen = sizeof(MeshPacket);
    _txChar->setValue((const uint8_t*)&pkt, totalLen);
    _txChar->notify();
}

bool BLEHandler::receivePacket(MeshPacket& pkt) {
    if (_rxHead == _rxTail) return false;
    pkt = _rxQueue[_rxTail];
    _rxTail = (_rxTail + 1) % RX_QUEUE_SIZE;
    return true;
}

void BLEHandler::update() {
    uint32_t now = millis();
    if (now - _lastScanMs >= BLE_SCAN_INTERVAL_MS) {
        _lastScanMs = now;
        startScan();
    }
}

size_t BLEHandler::connectedPeers() const {
    return _server ? _server->getConnectedCount() : 0;
}

// -------------------------------------------------------------------
// Private helpers
// -------------------------------------------------------------------

void BLEHandler::startAdvertising(const char* name) {
    NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
    adv->addServiceUUID(BLE_SERVICE_UUID);
    adv->setScanResponse(true);
    adv->setMinPreferred(0x06);
    NimBLEDevice::startAdvertising();
}

void BLEHandler::startScan() {
    _scan = NimBLEDevice::getScan();
    _scan->setActiveScan(true);
    _scan->setInterval(100);
    _scan->setWindow(99);
    // Non-blocking scan: callback-driven (results processed via advertising data)
    _scan->start(3, false);   // scan for 3 seconds, don't block
}
