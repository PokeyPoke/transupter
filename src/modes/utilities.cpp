#include "utilities.h"
#include "../config.h"
#include <M5Cardputer.h>
#include <WiFi.h>
#include <time.h>

UtilitiesMode::UtilitiesMode(Display& disp, Keyboard& kb, Battery& bat)
    : _disp(disp), _kb(kb), _bat(bat) {}

void UtilitiesMode::tick(AppState& state) {
    auto ks = _kb.poll();
    if (ks.isUp) {
        _view = (View)(((int)_view - 1 + 3) % 3);
    } else if (ks.isDown) {
        _view = (View)(((int)_view + 1) % 3);
    }
    drawCurrentView(state);
}

void UtilitiesMode::drawCurrentView(const AppState& state) {
    _disp.clearContent();
    auto& tft = M5Cardputer.Display;
    tft.setTextSize(1);

    switch (_view) {
    case View::Clock: {
        struct tm ti;
        time_t now = time(nullptr);
        localtime_r(&now, &ti);
        char buf[32];
        strftime(buf, sizeof(buf), "%H:%M:%S", &ti);
        _disp.printCentered(buf, Display::CONTENT_Y + 20, 3);
        strftime(buf, sizeof(buf), "%Y-%m-%d", &ti);
        _disp.printCentered(buf, Display::CONTENT_Y + 60, 1);
        break;
    }
    case View::BatteryStats: {
        auto bs = _bat.read();
        tft.setTextColor(WHITE, BLACK);
        tft.setCursor(4, Display::CONTENT_Y + 8);
        tft.printf("Battery: %d%%", bs.percent);
        tft.setCursor(4, Display::CONTENT_Y + 24);
        tft.printf("Charging: %s", bs.charging ? "Yes" : "No");
        break;
    }
    case View::SystemInfo: {
        tft.setTextColor(WHITE, BLACK);
        tft.setCursor(4, Display::CONTENT_Y + 8);
        tft.printf("WiFi: %s", state.wifiConnected ? WiFi.localIP().toString().c_str() : "Off");
        tft.setCursor(4, Display::CONTENT_Y + 22);
        tft.printf("RSSI: %d dBm", state.wifiRssi);
        tft.setCursor(4, Display::CONTENT_Y + 36);
        tft.printf("Free heap: %u KB", ESP.getFreeHeap() / 1024);
        tft.setCursor(4, Display::CONTENT_Y + 50);
        tft.printf("PSRAM free: %u KB", ESP.getFreePsram() / 1024);
        break;
    }
    }

    // Nav hint
    tft.setTextColor(DARKGREY, BLACK);
    tft.setCursor(4, Display::CONTENT_Y + 96);
    tft.print("Fn+W/S=switch view");
}
