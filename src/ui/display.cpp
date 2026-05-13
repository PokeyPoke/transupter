#include "display.h"
#include "../config.h"
#include <M5Cardputer.h>

static auto& tft = M5Cardputer.Display;

void Display::begin() {
    tft.setRotation(1);
    tft.fillScreen(BLACK);
    tft.setTextColor(WHITE, BLACK);
}

void Display::clear() {
    tft.fillScreen(BLACK);
}

void Display::showBootScreen(const char* message) {
    tft.fillScreen(BLACK);
    tft.setTextSize(2);
    tft.setCursor(10, 50);
    tft.setTextColor(CYAN, BLACK);
    tft.print("Transupter");
    tft.setTextColor(WHITE, BLACK);
    tft.setTextSize(1);
    tft.setCursor(10, 80);
    tft.print(message);
}

void Display::drawStatusBar(const AppState& state) {
    // Background strip
    tft.fillRect(0, 0, DISPLAY_WIDTH, STATUS_BAR_H, DARKGREY);
    tft.setTextSize(1);

    // WiFi icon
    tft.setCursor(4, 5);
    tft.setTextColor(state.wifiConnected ? GREEN : RED, DARKGREY);
    tft.print(state.wifiConnected ? "WiFi" : "NoWi");

    // Mode label
    tft.setCursor(50, 5);
    tft.setTextColor(WHITE, DARKGREY);
    switch (state.mode) {
        case AppMode::Translator: tft.print("TRANSLATE"); break;
        case AppMode::WifiSetup:  tft.print("WIFI SETUP"); break;
        case AppMode::Utilities:  tft.print("UTILITIES"); break;
        default:                  tft.print("BOOTING"); break;
    }

    // Battery
    tft.setCursor(185, 5);
    tft.setTextColor(state.batteryPct < BATTERY_LOW_WARN ? ORANGE : WHITE, DARKGREY);
    tft.printf("%d%%%s", state.batteryPct, state.batteryCharging ? "+" : "");

    tft.setTextColor(WHITE, BLACK); // reset
}

void Display::clearContent() {
    tft.fillRect(0, CONTENT_Y, DISPLAY_WIDTH, DISPLAY_HEIGHT - CONTENT_Y, BLACK);
}

void Display::printContent(const char* text, int textSize) {
    tft.setTextSize(textSize);
    tft.setTextColor(WHITE, BLACK);
    tft.setCursor(4, CONTENT_Y + 4);
    tft.print(text);
}

void Display::printCentered(const char* text, int y, int textSize) {
    tft.setTextSize(textSize);
    int charW = 6 * textSize;
    int x = (DISPLAY_WIDTH - strlen(text) * charW) / 2;
    if (x < 0) x = 0;
    tft.setCursor(x, y);
    tft.setTextColor(WHITE, BLACK);
    tft.print(text);
}

void Display::showPopup(const char* title, const char* body) {
    int bx = 20, by = 30, bw = 200, bh = 80;
    tft.fillRoundRect(bx, by, bw, bh, 6, DARKGREY);
    tft.drawRoundRect(bx, by, bw, bh, 6, WHITE);
    tft.setTextColor(YELLOW, DARKGREY);
    tft.setTextSize(1);
    tft.setCursor(bx + 6, by + 6);
    tft.print(title);
    tft.setTextColor(WHITE, DARKGREY);
    tft.setCursor(bx + 6, by + 22);
    tft.print(body);
}
