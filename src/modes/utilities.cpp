#include "utilities.h"
#include "../config.h"
#include <M5Cardputer.h>
#include <WiFi.h>
#include <time.h>

UtilitiesMode::UtilitiesMode(Display& disp, Keyboard& kb, Battery& bat, History& hist)
    : _disp(disp), _kb(kb), _bat(bat), _hist(hist) {}

bool UtilitiesMode::wifiSetupRequested() {
    if (_wifiSetupReq) { _wifiSetupReq = false; return true; }
    return false;
}

void UtilitiesMode::tick(AppState& state) {
    auto ks = _kb.poll();

    if (_view == View::HistoryLog) {
        auto& entries = _hist.entries();
        if (ks.isUp   && _histScroll < (int)entries.size() - 1) _histScroll++;
        if (ks.isDown && _histScroll > 0) _histScroll--;
        if (ks.isEsc) { _view = View::Clock; drawCurrentView(state); return; }
    } else if (_view == View::WiFiSetup) {
        bool enterNow = M5Cardputer.Keyboard.keysState().enter;
        bool enterPressed = enterNow && !_enterDown;
        _enterDown = enterNow;
        if (enterPressed) { _wifiSetupReq = true; return; }
        if (ks.isEsc)   { _view = View::Clock; }
        if (ks.isUp)    _view = (View)(((int)_view - 1 + 5) % 5);
        if (ks.isDown)  _view = (View)(((int)_view + 1) % 5);
    } else {
        if (ks.isUp)   _view = (View)(((int)_view - 1 + 5) % 5);
        if (ks.isDown) _view = (View)(((int)_view + 1) % 5);
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
    case View::HistoryLog: {
        auto& entries = _hist.entries();
        tft.setTextColor(CYAN, BLACK);
        tft.setCursor(4, Display::CONTENT_Y + 2);
        tft.printf("History (%d entries):", (int)entries.size());
        int maxVisible = 4;
        int start = max(0, (int)entries.size() - maxVisible - _histScroll);
        for (int i = 0; i < maxVisible && start + i < (int)entries.size(); i++) {
            auto& e = entries[start + i];
            tft.setTextColor(DARKGREY, BLACK);
            tft.setCursor(4, Display::CONTENT_Y + 14 + i * 24);
            tft.print(e.source.substring(0, 34));
            tft.setTextColor(WHITE, BLACK);
            tft.setCursor(4, Display::CONTENT_Y + 24 + i * 24);
            tft.print(e.translated.substring(0, 34));
        }
        if (entries.empty()) {
            tft.setTextColor(DARKGREY, BLACK);
            tft.setCursor(4, Display::CONTENT_Y + 30);
            tft.print("No translations yet.");
        }
        break;
    }
    case View::WiFiSetup: {
        tft.setTextColor(CYAN, BLACK);
        tft.setCursor(4, Display::CONTENT_Y + 8);
        tft.print("WiFi Setup");
        tft.setTextColor(WHITE, BLACK);
        tft.setCursor(4, Display::CONTENT_Y + 24);
        tft.printf("Status: %s", state.wifiConnected ? "Connected" : "Disconnected");
        if (state.wifiConnected) {
            tft.setCursor(4, Display::CONTENT_Y + 38);
            tft.printf("IP: %s", WiFi.localIP().toString().c_str());
            tft.setCursor(4, Display::CONTENT_Y + 52);
            tft.printf("SSID: %s", WiFi.SSID().c_str());
        }
        tft.setTextColor(YELLOW, BLACK);
        tft.setCursor(4, Display::CONTENT_Y + 72);
        tft.print("Enter = scan & reconnect");
        break;
    }
    }

    tft.setTextColor(DARKGREY, BLACK);
    tft.setCursor(4, Display::CONTENT_Y + 96);
    tft.print(";/. = switch view  Fn=back");
}
