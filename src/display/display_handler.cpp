// ============================================================
// Display Handler Implementation
// Board selection via compile-time flags:
//   -DHAS_OLED  → Adafruit SSD1306 (Heltec)
//   -DHAS_TFT   → TFT_eSPI (T-Deck)
// ============================================================

#include "display_handler.h"

#ifdef HAS_OLED
  #include <Wire.h>
  #include <Adafruit_GFX.h>
  #include <Adafruit_SSD1306.h>
  static Adafruit_SSD1306 oled(OLED_WIDTH, OLED_HEIGHT, &Wire, OLED_RST);
#endif

#ifdef HAS_TFT
  #include <TFT_eSPI.h>
  static TFT_eSPI tft;
#endif

DisplayHandler displayHandler;

// -------------------------------------------------------------------
// Colours (TFT only; OLED ignores colour)
// -------------------------------------------------------------------
#define COLOR_BG      0x0000   // black
#define COLOR_FG      0xFFFF   // white
#define COLOR_GREEN   0x07E0
#define COLOR_RED     0xF800
#define COLOR_YELLOW  0xFFE0
#define COLOR_BLUE    0x001F
#define COLOR_GRAY    0x7BEF

bool DisplayHandler::begin() {
#ifdef HAS_OLED
    Wire.begin(OLED_SDA, OLED_SCL);
    if (!oled.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Serial.println("[Display] OLED init FAILED");
        return false;
    }
    oled.clearDisplay();
    oled.setTextColor(SSD1306_WHITE);
    oled.setTextSize(1);
    oled.setCursor(0, 0);
    oled.println("MeshChat");
    oled.display();
    Serial.println("[Display] OLED ready");
    return true;
#endif

#ifdef HAS_TFT
    tft.init();
    tft.setRotation(1);
    tft.fillScreen(COLOR_BG);
    tft.setTextColor(COLOR_FG, COLOR_BG);
    tft.setTextSize(2);
    tft.setCursor(10, 80);
    tft.println("MeshChat");
    tft.setTextSize(1);
    tft.setCursor(10, 110);
    tft.println("Initialising...");
    Serial.println("[Display] TFT ready");
    return true;
#endif

    Serial.println("[Display] No display configured");
    return false;
}

void DisplayHandler::drawChat(const char* const* messages, uint8_t count) {
#ifdef HAS_OLED
    oled.clearDisplay();
    // Status bar at top (8 px)
    oled.drawFastHLine(0, 8, OLED_WIDTH, SSD1306_WHITE);
    // Chat lines below status bar
    int lineH = 8;
    int maxLines = (OLED_HEIGHT - 10) / lineH;
    int start = (count > maxLines) ? count - maxLines : 0;
    for (int i = start; i < count; i++) {
        oled.setCursor(0, 10 + (i - start) * lineH);
        oled.println(messages[i]);
    }
    oled.display();
#endif

#ifdef HAS_TFT
    tft.fillRect(0, 20, TFT_WIDTH, TFT_HEIGHT - 20, COLOR_BG);
    int lineH = 16;
    int maxLines = (TFT_HEIGHT - 20) / lineH;
    int start = (count > maxLines) ? count - maxLines : 0;
    for (int i = start; i < count; i++) {
        tft.setCursor(2, 22 + (i - start) * lineH);
        tft.setTextColor(COLOR_FG, COLOR_BG);
        tft.print(messages[i]);
    }
#endif
}

void DisplayHandler::drawNodeList(const char* const* names,
                                   const int16_t* rssi,
                                   const uint8_t* hops, uint8_t count) {
#ifdef HAS_OLED
    oled.clearDisplay();
    oled.setCursor(0, 0);
    oled.println("== Nodes ==");
    for (int i = 0; i < count && i < 6; i++) {
        oled.printf("%s  %ddBm  %dh\n", names[i], rssi[i], hops[i]);
    }
    oled.display();
#endif

#ifdef HAS_TFT
    tft.fillScreen(COLOR_BG);
    tft.setCursor(2, 2);
    tft.setTextColor(COLOR_GREEN, COLOR_BG);
    tft.print("== Nodes ==");
    for (int i = 0; i < count; i++) {
        int y = 20 + i * 18;
        tft.setCursor(2, y);
        tft.setTextColor(COLOR_FG, COLOR_BG);
        tft.printf("%-12s %4d dBm  %dh", names[i], rssi[i], hops[i]);
    }
#endif
}

void DisplayHandler::drawStatusBar(bool loraOk, bool bleOk, bool wifiOk,
                                    int loraRSSI, uint8_t battPct) {
#ifdef HAS_OLED
    drawStatusBarOLED(loraOk, bleOk, wifiOk, loraRSSI, battPct);
#endif
#ifdef HAS_TFT
    drawStatusBarTFT(loraOk, bleOk, wifiOk, loraRSSI, battPct);
#endif
}

void DisplayHandler::showToast(const char* text, uint16_t durationMs) {
    strncpy(_toastText, text, sizeof(_toastText) - 1);
    _toastUntil = millis() + durationMs;

#ifdef HAS_OLED
    oled.fillRect(0, OLED_HEIGHT - 10, OLED_WIDTH, 10, SSD1306_BLACK);
    oled.setCursor(0, OLED_HEIGHT - 9);
    oled.println(text);
    oled.display();
#endif

#ifdef HAS_TFT
    tft.fillRect(0, TFT_HEIGHT - 18, TFT_WIDTH, 18, COLOR_BLUE);
    tft.setCursor(2, TFT_HEIGHT - 16);
    tft.setTextColor(COLOR_FG, COLOR_BLUE);
    tft.print(text);
#endif
}

void DisplayHandler::refresh() {
#ifdef HAS_OLED
    oled.display();
#endif
    // TFT is immediate-mode; no explicit flush needed.
}

void DisplayHandler::update() {
    // Clear expired toast
    if (_toastUntil > 0 && millis() > _toastUntil) {
        _toastUntil = 0;
        _toastText[0] = '\0';
#ifdef HAS_OLED
        oled.fillRect(0, OLED_HEIGHT - 10, OLED_WIDTH, 10, SSD1306_BLACK);
        oled.display();
#endif
#ifdef HAS_TFT
        tft.fillRect(0, TFT_HEIGHT - 18, TFT_WIDTH, 18, COLOR_BG);
#endif
    }
}

// -------------------------------------------------------------------
// Private helpers
// -------------------------------------------------------------------

void DisplayHandler::drawStatusBarOLED(bool loraOk, bool bleOk, bool wifiOk,
                                        int loraRSSI, uint8_t battPct) {
#ifdef HAS_OLED
    oled.fillRect(0, 0, OLED_WIDTH, 8, SSD1306_BLACK);
    oled.setCursor(0, 0);
    oled.print(loraOk  ? "L" : "l");
    oled.print(bleOk   ? "B" : "b");
    oled.print(wifiOk  ? "W" : "w");
    oled.printf(" %ddBm  %d%%", loraRSSI, battPct);
    oled.display();
#endif
}

void DisplayHandler::drawStatusBarTFT(bool loraOk, bool bleOk, bool wifiOk,
                                       int loraRSSI, uint8_t battPct) {
#ifdef HAS_TFT
    tft.fillRect(0, 0, TFT_WIDTH, 18, COLOR_GRAY);
    int x = 2;
    // LoRa indicator
    tft.setCursor(x, 2); tft.setTextColor(loraOk ? COLOR_GREEN : COLOR_RED, COLOR_GRAY);
    tft.print("LoRa"); x += 40;
    // BLE indicator
    tft.setCursor(x, 2); tft.setTextColor(bleOk ? COLOR_GREEN : COLOR_RED, COLOR_GRAY);
    tft.print("BLE"); x += 36;
    // WiFi indicator
    tft.setCursor(x, 2); tft.setTextColor(wifiOk ? COLOR_GREEN : COLOR_RED, COLOR_GRAY);
    tft.print("WiFi"); x += 48;
    // RSSI
    tft.setCursor(x, 2); tft.setTextColor(COLOR_FG, COLOR_GRAY);
    tft.printf("%ddBm  Bat:%d%%", loraRSSI, battPct);
#endif
}
