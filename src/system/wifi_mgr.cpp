#include "wifi_mgr.h"
#include "../config.h"
#include <WiFi.h>

void WifiMgr::connect(const String& ssid, const String& pass) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), pass.c_str());
    _status  = WifiStatus::Connecting;
    _startMs = millis();
}

void WifiMgr::tick() {
    if (_status != WifiStatus::Connecting) return;

    if (WiFi.status() == WL_CONNECTED) {
        _status = WifiStatus::Connected;
        return;
    }
    if (millis() - _startMs > WIFI_CONNECT_TIMEOUT_MS) {
        _status = WifiStatus::Failed;
        WiFi.disconnect(true);
    }
}

void WifiMgr::disconnect() {
    WiFi.disconnect(true);
    _status = WifiStatus::Idle;
}

WifiStatus WifiMgr::status() const { return _status; }

int WifiMgr::rssi() const {
    return WiFi.isConnected() ? WiFi.RSSI() : 0;
}

bool WifiMgr::isConnected() const { return _status == WifiStatus::Connected; }

std::vector<AccessPoint> WifiMgr::scan() {
    std::vector<AccessPoint> results;
    int n = WiFi.scanNetworks();
    for (int i = 0; i < n; i++) {
        results.push_back({ WiFi.SSID(i), WiFi.RSSI(i) });
    }
    WiFi.scanDelete();
    return results;
}
