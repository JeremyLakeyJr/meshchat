#pragma once

// ============================================================
// Display Handler - unified display driver for OLED (Heltec)
// and TFT (T-Deck).  Renders the chat UI, node list, and
// radio status bar.
// ============================================================

#include <Arduino.h>
#include "../config.h"

// Forward-declare display types to avoid including heavy headers here.
// Implementations select the right type via #ifdef.
class DisplayHandler {
public:
    // Initialise the display hardware.
    bool begin();

    // Draw the main chat screen.
    // messages: pointer to array of null-terminated strings (newest last)
    // count:    number of entries
    void drawChat(const char* const* messages, uint8_t count);

    // Draw the node list screen.
    void drawNodeList(const char* const* names, const int16_t* rssi,
                      const uint8_t* hops, uint8_t count);

    // Draw the status bar (radio, BLE, WiFi indicators + battery).
    void drawStatusBar(bool loraOk, bool bleOk, bool wifiOk,
                       int loraRSSI, uint8_t battPct);

    // Show a short notification / toast for durationMs.
    void showToast(const char* text, uint16_t durationMs = 2000);

    // Force a display refresh.
    void refresh();

    // Must be called from the main loop (handles toast expiry, etc.).
    void update();

    // Current screen index (0=chat, 1=nodes, 2=settings).
    uint8_t screen() const { return _screen; }
    void setScreen(uint8_t s) { _screen = s; }

private:
    uint8_t  _screen     = 0;
    uint32_t _toastUntil = 0;
    char     _toastText[64] = {};

    void drawStatusBarOLED(bool loraOk, bool bleOk, bool wifiOk,
                            int loraRSSI, uint8_t battPct);
    void drawStatusBarTFT(bool loraOk, bool bleOk, bool wifiOk,
                           int loraRSSI, uint8_t battPct);
};

extern DisplayHandler displayHandler;
