#include "wifi_setup.h"
#include "../config.h"
#include <M5Cardputer.h>

WifiSetupMode::WifiSetupMode(Display& disp, Keyboard& kb, WifiMgr& wifi, NvsStore& nvs)
    : _disp(disp), _kb(kb), _wifi(wifi), _nvs(nvs) {}

bool WifiSetupMode::tick(AppState& state) {
    switch (_step) {

    case Step::Scanning:
        _disp.clearContent();
        _disp.printContent("Scanning WiFi...");
        _aps      = _wifi.scan();
        _selected = 0;
        _step     = Step::SelectNetwork;
        drawNetworkList();
        break;

    case Step::SelectNetwork: {
        static constexpr int MAX_VISIBLE = 7;
        auto ks = _kb.poll();
        if (ks.isEsc) return true;  // Fn/Opt = skip WiFi setup entirely
        if (ks.isUp && _selected > 0) {
            _selected--;
            if (_selected < _scrollOffset) _scrollOffset = _selected;
            drawNetworkList();
        }
        if (ks.isDown && _selected < (int)_aps.size() - 1) {
            _selected++;
            if (_selected >= _scrollOffset + MAX_VISIBLE) _scrollOffset = _selected - MAX_VISIBLE + 1;
            drawNetworkList();
        }
        // Rising-edge Enter detection: trigger once per press
        bool enterNow = M5Cardputer.Keyboard.keysState().enter;
        bool enterPressed = enterNow && !_enterDown;
        _enterDown = enterNow;
        if (enterPressed && !_aps.empty()) {
            _password = "";
            _step = Step::EnterPassword;
            drawPasswordEntry();
        }
        break;
    }

    case Step::EnterPassword: {
        auto ks = _kb.poll();
        bool enterNow = M5Cardputer.Keyboard.keysState().enter;
        bool enterPressed = enterNow && !_enterDown;
        _enterDown = enterNow;
        if (ks.isDel && _password.length() > 0) {
            _password.remove(_password.length() - 1);
            drawPasswordEntry();
        } else if (ks.ch && !enterNow && !ks.isDel) {
            _password += ks.ch;
            drawPasswordEntry();
        } else if (enterPressed) {
            _wifi.connect(_aps[_selected].ssid, _password);
            _step = Step::Connecting;
            drawConnecting();
        } else if (ks.isEsc) {
            _step = Step::SelectNetwork;
            drawNetworkList();
        }
        break;
    }

    case Step::Connecting:
        _wifi.tick();
        if (_wifi.isConnected()) {
            _nvs.begin();
            _nvs.putString(NVS_WIFI_SSID, _aps[_selected].ssid);
            _nvs.putString(NVS_WIFI_PASS, _password);
            _nvs.end();
            state.wifiConnected = true;
            return true; // done — caller switches mode
        }
        if (_wifi.status() == WifiStatus::Failed) {
            _disp.showPopup("Connect failed", "Press any key to retry");
            _step = Step::SelectNetwork;
        }
        break;
    }
    return false;
}

void WifiSetupMode::drawNetworkList() {
    _disp.clearContent();
    auto& tft = M5Cardputer.Display;
    tft.setTextSize(1);
    tft.setTextColor(CYAN, BLACK);
    tft.setCursor(4, Display::CONTENT_Y + 2);
    tft.print(";/. scroll  Enter=select  Fn=skip");

    if (_aps.empty()) {
        tft.setTextColor(ORANGE, BLACK);
        tft.setCursor(4, Display::CONTENT_Y + 20);
        tft.print("No networks found.");
        tft.setTextColor(DARKGREY, BLACK);
        tft.setCursor(4, Display::CONTENT_Y + 36);
        tft.print("Navigate away & back to retry.");
        return;
    }

    static constexpr int MAX_VISIBLE = 7;
    for (int i = 0; i < MAX_VISIBLE && _scrollOffset + i < (int)_aps.size(); i++) {
        int idx = _scrollOffset + i;
        tft.setCursor(4, Display::CONTENT_Y + 14 + i * 14);
        tft.setTextColor(idx == _selected ? YELLOW : WHITE, BLACK);
        tft.printf("%s (%d)", _aps[idx].ssid.c_str(), _aps[idx].rssi);
    }

    // Scroll indicator
    if ((int)_aps.size() > MAX_VISIBLE) {
        tft.setTextColor(DARKGREY, BLACK);
        tft.setCursor(220, Display::CONTENT_Y + 14);
        tft.printf("%d/%d", _selected + 1, (int)_aps.size());
    }
}

void WifiSetupMode::drawPasswordEntry() {
    _disp.clearContent();
    auto& tft = M5Cardputer.Display;
    tft.setTextSize(1);
    tft.setTextColor(CYAN, BLACK);
    tft.setCursor(4, Display::CONTENT_Y + 4);
    tft.printf("Password for %s:", _aps[_selected].ssid.c_str());
    tft.setTextColor(WHITE, BLACK);
    tft.setCursor(4, Display::CONTENT_Y + 20);
    String masked = "";
    for (int i = 0; i < (int)_password.length(); i++) masked += '*';
    tft.print(masked);
    tft.setTextColor(DARKGREY, BLACK);
    tft.setCursor(4, Display::CONTENT_Y + 36);
    tft.print("Enter=connect  Del=backspace  Fn=back");
}

void WifiSetupMode::drawConnecting() {
    _disp.clearContent();
    _disp.printContent("Connecting...");
}
