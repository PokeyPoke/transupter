#pragma once
#include <Arduino.h>

enum class AppMode {
    Booting,
    WifiSetup,
    Translator,
    Utilities,
};

struct AppState {
    AppMode mode = AppMode::Booting;

    // WiFi
    bool wifiConnected   = false;
    int  wifiRssi        = 0;

    // Battery
    int  batteryPct      = 100;
    bool batteryCharging = false;

    // API keys (loaded from NVS at boot)
    String keyGroq;
    String keyAnthropic;
    String keyOpenAI;

    // Flags
    bool apiKeysSet      = false;
    bool lowBatteryShown = false;
};
