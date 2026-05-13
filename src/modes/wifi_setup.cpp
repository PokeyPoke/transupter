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
        _aps  = _wifi.scan();
        _step = Step::SelectNetwork;
        drawNetworkList();
        break;

    case Step::SelectNetwork: {
        auto ks = _kb.poll();
        if (ks.isUp && _selected > 0) { _selected--; drawNetworkList(); }
        if (ks.isDown && _selected < (int)_aps.size()-1) { _selected++; drawNetworkList(); }
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
    tft.print("Select network (Fn+W/S, Enter):");
    for (int i = 0; i < (int)_aps.size() && i < 5; i++) {
        tft.setCursor(4, Display::CONTENT_Y + 14 + i * 12);
        tft.setTextColor(i == _selected ? YELLOW : WHITE, BLACK);
        tft.printf("%s (%ddBm)", _aps[i].ssid.c_str(), _aps[i].rssi);
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
