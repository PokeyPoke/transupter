#include "wifi_setup.h"
#include "../config.h"
#include <M5Cardputer.h>
#include <WiFi.h>

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
        if (ks.isEsc) return true;          // Fn = skip
        if (ks.isOpt) { startWebPortal(); break; } // Opt = phone/computer portal
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
            drawNetworkList();
        }
        break;

    case Step::WebPortal:
        _server.handleClient();
        if (_portalDone) {
            _server.stop();
            WiFi.softAPdisconnect(true);
            WiFi.mode(WIFI_STA);
            _wifi.connect(_portalSsid, _portalPass);
            _step = Step::WebConnecting;
            drawConnecting();
        }
        break;

    case Step::WebConnecting:
        _wifi.tick();
        if (_wifi.isConnected()) {
            _nvs.begin();
            _nvs.putString(NVS_WIFI_SSID, _portalSsid);
            _nvs.putString(NVS_WIFI_PASS, _portalPass);
            _nvs.end();
            state.wifiConnected = true;
            return true;
        }
        if (_wifi.status() == WifiStatus::Failed) {
            _disp.showPopup("Connect failed", "Fn to skip");
            _step = Step::SelectNetwork;
            drawNetworkList();
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
    tft.print(";/.=scroll  Enter=select  Opt=phone  Fn=skip");

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

// ── Web Portal ────────────────────────────────────────────────────────────────

void WifiSetupMode::startWebPortal() {
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP("Transupter-WiFi");

    _server.on("/", HTTP_GET,  [this]() { _server.send(200, "text/html", buildPortalHtml()); });
    _server.on("/connect", HTTP_POST, [this]() {
        _portalSsid = _server.arg("ssid");
        _portalPass = _server.arg("password");

        if (_portalSsid.isEmpty()) {
            _server.send(400, "text/html",
                "<html><body style='font-family:sans-serif;background:#1a1a2e;color:#eee;padding:20px'>"
                "<h2 style='color:#ff4444'>SSID required</h2>"
                "<a href='/' style='color:#00d4ff'>Back</a></body></html>");
            return;
        }

        _server.send(200, "text/html",
            "<html><head><meta name='viewport' content='width=device-width,initial-scale=1'></head>"
            "<body style='font-family:sans-serif;background:#1a1a2e;color:#eee;padding:20px;text-align:center'>"
            "<h2 style='color:#00ff88'>&#10003; Connecting...</h2>"
            "<p>The Cardputer is connecting to <b>" + _portalSsid + "</b>.<br>You can close this tab.</p>"
            "</body></html>");

        _portalDone = true;
    });
    _server.begin();
    _step = Step::WebPortal;
    drawWebPortalScreen();
}

void WifiSetupMode::drawWebPortalScreen() {
    _disp.clearContent();
    auto& tft = M5Cardputer.Display;
    tft.setTextSize(1);
    tft.setTextColor(CYAN, BLACK);
    tft.setCursor(4, Display::CONTENT_Y + 8);
    tft.print("Connect phone/laptop to WiFi:");
    tft.setTextColor(WHITE, BLACK);
    tft.setCursor(4, Display::CONTENT_Y + 22);
    tft.print("> Transupter-WiFi");
    tft.setTextColor(CYAN, BLACK);
    tft.setCursor(4, Display::CONTENT_Y + 40);
    tft.print("Then open browser:");
    tft.setTextColor(WHITE, BLACK);
    tft.setCursor(4, Display::CONTENT_Y + 54);
    tft.print("> 192.168.4.1");
    tft.setTextColor(DARKGREY, BLACK);
    tft.setCursor(4, Display::CONTENT_Y + 80);
    tft.print("Waiting...");
}

String WifiSetupMode::buildPortalHtml() const {
    String html =
        "<!DOCTYPE html><html><head>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<title>Transupter WiFi</title>"
        "<style>"
        "body{font-family:sans-serif;background:#1a1a2e;color:#eee;max-width:480px;margin:40px auto;padding:20px}"
        "h1{color:#00d4ff}label{display:block;margin-bottom:4px;font-size:13px;color:#aaa}"
        "select,input{width:100%;padding:12px;background:#16213e;border:1px solid #0f3460;"
        "border-radius:6px;color:#eee;font-size:14px;box-sizing:border-box;margin-bottom:16px}"
        "button{width:100%;padding:14px;background:#00d4ff;color:#000;border:none;"
        "border-radius:6px;font-size:16px;font-weight:bold;cursor:pointer}"
        "</style></head><body>"
        "<h1>Transupter WiFi</h1>"
        "<form method='POST' action='/connect'>"
        "<label>Network</label>"
        "<select name='ssid'>";

    for (auto& ap : _aps) {
        html += "<option value='" + ap.ssid + "'>" + ap.ssid
              + " (" + String(ap.rssi) + " dBm)</option>";
    }

    html +=
        "</select>"
        "<label>Password</label>"
        "<input type='password' name='password' placeholder='Leave blank if open network'>"
        "<button type='submit'>Connect &#8594;</button>"
        "</form></body></html>";

    return html;
}

