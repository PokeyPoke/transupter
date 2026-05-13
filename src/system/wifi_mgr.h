#pragma once
#include <Arduino.h>
#include <vector>

struct AccessPoint {
    String ssid;
    int    rssi;
};

enum class WifiStatus { Idle, Connecting, Connected, Failed };

class WifiMgr {
public:
    // Attempt to connect; non-blocking. Call tick() in loop to advance state.
    void        connect(const String& ssid, const String& pass);
    void        tick();          // advance connection FSM
    void        disconnect();

    WifiStatus  status()  const;
    int         rssi()    const;
    bool        isConnected() const;

    // Blocking scan (call from setup or a dedicated task, ~2-3s)
    std::vector<AccessPoint> scan();

private:
    WifiStatus _status   = WifiStatus::Idle;
    unsigned long _startMs = 0;
};
