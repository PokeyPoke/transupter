#include "wifi_mgr.h"
#include "../config.h"
#include <WiFi.h>
#include <lwip/dns.h>
#include <lwip/ip_addr.h>

static void setGoogleDNS() {
    ip_addr_t dns;
    IP4_ADDR(&dns.u_addr.ip4, 8, 8, 8, 8);
    dns.type = IPADDR_TYPE_V4;
    dns_setserver(0, &dns);
    IP4_ADDR(&dns.u_addr.ip4, 8, 8, 4, 4);
    dns_setserver(1, &dns);
}

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
        setGoogleDNS(); // override router DNS to bypass content filtering
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
    // Reset radio to clean STA state before scanning
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(true);
    delay(150);

    std::vector<AccessPoint> results;
    int n = WiFi.scanNetworks();
    for (int i = 0; i < n; i++) {
        results.push_back({ WiFi.SSID(i), WiFi.RSSI(i) });
    }
    WiFi.scanDelete();
    return results;
}
