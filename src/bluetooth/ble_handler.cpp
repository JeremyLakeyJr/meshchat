// ============================================================
// BLE Handler Implementation (NimBLE-Arduino)
// Supports two firmware modes, selected at runtime via
// currentFirmwareMode (persisted in NVS):
//
//   FirmwareMode::Meshtastic – Advertises the Meshtastic BLE GATT
//     service with ToRadio / FromRadio / FromNum characteristics,
//     matching the Meshtastic device BLE API so that Meshtastic
//     phone apps can discover and communicate with this node.
//
//   FirmwareMode::BitChat    – Advertises the Nordic UART Service
//     (NUS) used by BitChat, with writable RX and notifiable TX
//     characteristics for peer-to-peer BLE mesh messaging.
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

    // Create service with the mode-appropriate UUID
    const char* svcUUID = (currentFirmwareMode == FirmwareMode::Meshtastic)
                          ? BLE_MESHTASTIC_SERVICE_UUID
                          : BLE_BITCHAT_SERVICE_UUID;
    NimBLEService* svc = _server->createService(svcUUID);

    if (currentFirmwareMode == FirmwareMode::Meshtastic) {
        // ── Meshtastic BLE GATT API ───────────────────────────────────────
        // FromRadio (TX characteristic): notify-able, device → phone.
        // Carries serialised Meshtastic FromRadio protobuf packets.
        _txChar = svc->createCharacteristic(
            BLE_MESHTASTIC_TX_CHAR_UUID,
            NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);

        // ToRadio (RX characteristic): writable, phone → device.
        // Carries serialised Meshtastic ToRadio protobuf packets.
        _rxChar = svc->createCharacteristic(
            BLE_MESHTASTIC_RX_CHAR_UUID,
            NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
        _rxChar->setCallbacks(new BLECharCallbacks(this));

        // FromNum: notify-only counter incremented when a new FromRadio
        // packet is waiting; Meshtastic apps use this to trigger a read.
        _fromNumChar = svc->createCharacteristic(
            BLE_MESHTASTIC_FROMNUM_UUID,
            NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
        uint32_t initialFromNum = 0;
        _fromNumChar->setValue((uint8_t*)&initialFromNum, sizeof(initialFromNum));

        Serial.printf("[BLE] Meshtastic service advertising as \"%s\"\n", nodeName);
    } else {
        // ── BitChat – Nordic UART Service (NUS) ──────────────────────────
        // TX characteristic: notify-able, device → phone.
        _txChar = svc->createCharacteristic(
            BLE_BITCHAT_TX_CHAR_UUID,
            NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);

        // RX characteristic: writable, phone → device.
        _rxChar = svc->createCharacteristic(
            BLE_BITCHAT_RX_CHAR_UUID,
            NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
        _rxChar->setCallbacks(new BLECharCallbacks(this));

        Serial.printf("[BLE] BitChat (NUS) service advertising as \"%s\"\n", nodeName);
    }

    svc->start();
    startAdvertising(nodeName);
    startScan();

    return true;
}

void BLEHandler::sendPacket(const MeshPacket& pkt) {
    if (!_txChar) return;
    const size_t headerLen = offsetof(MeshPacket, payload);
    size_t totalLen = headerLen + pkt.payload_len;
    if (totalLen > sizeof(MeshPacket)) totalLen = sizeof(MeshPacket);
    _txChar->setValue((const uint8_t*)&pkt, totalLen);
    _txChar->notify();

    if (currentFirmwareMode == FirmwareMode::Meshtastic) {
        // Increment the Meshtastic FromNum counter and notify connected clients
        // so that Meshtastic phone apps know a new FromRadio packet is ready.
        if (_fromNumChar) {
            _fromNum++;
            _fromNumChar->setValue((uint8_t*)&_fromNum, sizeof(_fromNum));
            _fromNumChar->notify();
        }
    }
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
    adv->addServiceUUID(currentFirmwareMode == FirmwareMode::Meshtastic
                        ? BLE_MESHTASTIC_SERVICE_UUID
                        : BLE_BITCHAT_SERVICE_UUID);
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
