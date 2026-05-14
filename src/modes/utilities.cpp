#include "utilities.h"
#include "../config.h"
#include <M5Cardputer.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <time.h>

UtilitiesMode::UtilitiesMode(Display& disp, Keyboard& kb, Battery& bat, History& hist)
    : _disp(disp), _kb(kb), _bat(bat), _hist(hist) {}

bool UtilitiesMode::wifiSetupRequested() {
    if (_wifiSetupReq) { _wifiSetupReq = false; return true; }
    return false;
}

bool UtilitiesMode::backRequested() {
    if (_backReq) { _backReq = false; return true; }
    return false;
}

void UtilitiesMode::tick(AppState& state) {
    auto ks = _kb.poll();

    // Rising-edge Fn/Opt detection — prevents immediate back-fire when entering from Translator
    auto& raw = M5Cardputer.Keyboard.keysState();
    bool fnNow = raw.fn || raw.opt;
    bool fnPressed = fnNow && !_fnDown;
    _fnDown = fnNow;
    if (fnPressed) { _backReq = true; return; }

    if (_view == View::HistoryLog) {
        auto& entries = _hist.entries();
        if (ks.isUp   && _histScroll < (int)entries.size() - 1) _histScroll++;
        if (ks.isDown && _histScroll > 0) _histScroll--;
        if (ks.isEsc) { _view = View::Clock; drawCurrentView(state); return; }
    } else {
        View prev = _view;
        if (ks.isUp)   _view = (View)(((int)_view - 1 + VIEW_COUNT) % VIEW_COUNT);
        if (ks.isDown) _view = (View)(((int)_view + 1) % VIEW_COUNT);
        // Auto-trigger WiFi setup the moment user navigates to that view
        if (_view == View::WiFiSetup && prev != View::WiFiSetup) {
            _wifiSetupReq = true;
        }
        // Auto-run Groq test when navigating to APITest view
        if (_view == View::APITest && prev != View::APITest) {
            runGroqTest(state);
        }
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
        tft.print("Navigate here to scan");
        break;
    }
    case View::APITest: {
        tft.setTextColor(CYAN, BLACK);
        tft.setCursor(4, Display::CONTENT_Y + 8);
        tft.print("Groq API Test");
        tft.setTextColor(WHITE, BLACK);
        tft.setCursor(4, Display::CONTENT_Y + 28);
        tft.print(_testResult.substring(0, 34));
        tft.setTextColor(YELLOW, BLACK);
        tft.setCursor(4, Display::CONTENT_Y + 72);
        tft.print("Navigate here to re-run");
        break;
    }
    }

    tft.setTextColor(DARKGREY, BLACK);
    tft.setCursor(4, Display::CONTENT_Y + 96);
    tft.print(";/. = switch view  Fn=back");
}

void UtilitiesMode::runGroqTest(AppState& state) {
    _testResult = "Connecting...";
    drawCurrentView(state);

    if (!state.wifiConnected) { _testResult = "No WiFi"; return; }
    if (state.keyGroq.isEmpty()) { _testResult = "No Groq key set"; return; }

    WiFiClientSecure client;
    client.setInsecure();

    if (!client.connect("api.groq.com", 443, 10000)) {
        _testResult = "TLS connect failed";
        return;
    }
    client.setTimeout(10);

    // Minimal GET to /openai/v1/models — tests key validity and connection
    client.printf("GET /openai/v1/models HTTP/1.1\r\n");
    client.printf("Host: api.groq.com\r\n");
    client.printf("Authorization: Bearer %s\r\n", state.keyGroq.c_str());
    client.print("User-Agent: transupter/1.0\r\n");
    client.print("Connection: close\r\n\r\n");
    client.flush();

    String statusLine = client.readStringUntil('\n');
    int code = 0;
    sscanf(statusLine.c_str(), "HTTP/1.1 %d", &code);
    client.stop();

    if      (code == 200) _testResult = "OK (200) key valid";
    else if (code == 401) _testResult = "401 Bad API key";
    else if (code == 403) _testResult = "403 Forbidden";
    else if (code == 429) _testResult = "429 Rate limited";
    else if (code == 0)   _testResult = "CONN_RESET (TLS ok)";
    else                  _testResult = "HTTP " + String(code);
}
