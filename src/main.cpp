#include <M5Cardputer.h>
#include <WiFi.h>
#include <LittleFS.h>
#include <time.h>

#include "config.h"
#include "app.h"
#include "system/nvs_store.h"
#include "system/wifi_mgr.h"
#include "system/battery.h"
#include "ui/display.h"
#include "ui/keyboard.h"
#include "modes/wifi_setup.h"
#include "modes/api_key_setup.h"
#include "modes/utilities.h"
#include "audio/wav.h"
#include "audio/recorder.h"
#include "audio/player.h"
#include "api/groq.h"
#include "api/anthropic.h"
#include "api/openai_tts.h"
#include "storage/history.h"
#include "modes/translator.h"

static AppState       state;
static NvsStore       nvs;
static WifiMgr        wifiMgr;
static Battery        battery;
static Display        disp;
static Keyboard       kb;

static WifiSetupMode*  wifiSetup  = nullptr;
static ApiKeySetupMode* apiSetup  = nullptr;
static UtilitiesMode*  utilities  = nullptr;
static History          history;
static TranslatorMode*  translator = nullptr;

static unsigned long lastStatusMs  = 0;
static unsigned long lastBatteryMs = 0;

// ──────────────────────────────────────────────────────────────────────
// Boot helpers
// ──────────────────────────────────────────────────────────────────────

static void bootNvs() {
    nvs.begin();
    state.keyGroq       = nvs.getString(NVS_KEY_GROQ,       "");
    state.keyAnthropic  = nvs.getString(NVS_KEY_ANTHROPIC,  "");
    state.keyOpenAI     = nvs.getString(NVS_KEY_OPENAI,     "");
    state.apiKeysSet    = !state.keyGroq.isEmpty() &&
                          !state.keyAnthropic.isEmpty() &&
                          !state.keyOpenAI.isEmpty();
    nvs.end();
}

static void bootWifi() {
    nvs.begin();
    String ssid = nvs.getString(NVS_WIFI_SSID, "");
    String pass = nvs.getString(NVS_WIFI_PASS, "");
    nvs.end();

    if (ssid.isEmpty()) {
        state.mode = AppMode::WifiSetup;
        wifiSetup  = new WifiSetupMode(disp, kb, wifiMgr, nvs);
        return;
    }

    // Attempt reconnect with skip countdown
    disp.showBootScreen("Connecting to WiFi...");
    wifiMgr.connect(ssid, pass);

    unsigned long start = millis();
    bool skipped = false;
    while (millis() - start < WIFI_SKIP_TIMEOUT_MS) {
        M5Cardputer.update();
        wifiMgr.tick();
        if (wifiMgr.isConnected()) break;
        if (M5Cardputer.Keyboard.isPressed()) { skipped = true; break; }

        // Countdown
        int remaining = (WIFI_SKIP_TIMEOUT_MS - (millis() - start)) / 1000;
        char buf[48];
        snprintf(buf, sizeof(buf), "Connecting... (%ds, any key to skip)", remaining);
        disp.showBootScreen(buf);
        delay(200);
    }

    state.wifiConnected = wifiMgr.isConnected();
    if (state.wifiConnected) {
        // Set Google DNS — avoids router DNS issues that block API domains
        WiFi.config(WiFi.localIP(), WiFi.gatewayIP(), WiFi.subnetMask(),
                    IPAddress(8, 8, 8, 8), IPAddress(8, 8, 4, 4));
    } else if (!skipped) {
        disp.showPopup("WiFi failed", "Continuing offline");
        delay(1500);
    }
}

static void syncNtp() {
    if (!state.wifiConnected) return;
    configTime(0, 0, "pool.ntp.org", "time.google.com");
    struct tm ti;
    unsigned long start = millis();
    while (!getLocalTime(&ti) && millis() - start < 2000) delay(100);
}

// ──────────────────────────────────────────────────────────────────────
void setup() {
    // Explicitly initialise PSRAM before anything else
    if (!psramInit()) {
        // PSRAM init failed — device will fall back to DRAM for audio buffers
    }

    auto cfg = M5.config();
    M5Cardputer.begin(cfg, true);

    // Mic and Speaker share I2S — stop speaker first, then start mic.
    // M5Cardputer.begin() does not auto-start the mic.
    M5Cardputer.Speaker.end();
    M5Cardputer.Mic.begin();

    disp.begin();

    LittleFS.begin(true); // format on first boot if needed
    history.begin();

    disp.showBootScreen("Loading settings...");
    bootNvs();

    bootWifi();
    syncNtp();

    // Battery first read
    auto bs = battery.read();
    state.batteryPct      = bs.percent;
    state.batteryCharging = bs.charging;

    // Decide mode
    if (state.mode != AppMode::WifiSetup) {
        if (!state.apiKeysSet) {
            apiSetup   = new ApiKeySetupMode(disp, kb, nvs);
            state.mode = AppMode::Booting; // API setup uses Booting slot
        } else {
            state.mode = AppMode::Translator;
        }
    }

    utilities = new UtilitiesMode(disp, kb, battery, history);
    if (state.apiKeysSet) {
        translator = new TranslatorMode(disp, kb, history);
    }

    disp.drawStatusBar(state);
}

// ──────────────────────────────────────────────────────────────────────
void loop() {
    M5Cardputer.update();

    // Refresh battery every 30s
    if (millis() - lastBatteryMs > 30000) {
        auto bs = battery.read();
        state.batteryPct      = bs.percent;
        state.batteryCharging = bs.charging;
        lastBatteryMs = millis();

        if (state.batteryPct <= BATTERY_LOW_POPUP && !state.lowBatteryShown) {
            disp.showPopup("Low Battery", "Charge soon!");
            state.lowBatteryShown = true;
            delay(1500);
        }
    }

    // Refresh status bar every second
    if (millis() - lastStatusMs > 1000) {
        if (state.wifiConnected) {
            wifiMgr.tick();
            state.wifiConnected = wifiMgr.isConnected();
            state.wifiRssi      = wifiMgr.rssi();
        }
        disp.drawStatusBar(state);
        lastStatusMs = millis();
    }

    // ── Mode dispatch ──────────────────────────────────────────────────
    switch (state.mode) {

    case AppMode::WifiSetup:
        if (wifiSetup && wifiSetup->tick(state)) {
            delete wifiSetup;
            wifiSetup = nullptr;
            if (!state.apiKeysSet) {
                apiSetup   = new ApiKeySetupMode(disp, kb, nvs);
                state.mode = AppMode::Booting;
            } else {
                state.mode = AppMode::Translator;
                disp.clearContent();
            }
        }
        break;

    case AppMode::Booting:
        // API key setup in progress
        if (apiSetup && apiSetup->tick(state)) {
            delete apiSetup;
            apiSetup   = nullptr;
            state.mode = AppMode::Translator;
            if (!translator) {
                translator = new TranslatorMode(disp, kb, history);
            }
            disp.clearContent();
        }
        break;

    case AppMode::Translator:
        if (translator) {
            translator->tick(state);
        } else {
            if (millis() % 5000 < 50) {
                disp.clearContent();
                disp.printContent("API keys missing.\nFn for Utilities.");
            }
        }
        {
            auto ks = kb.poll();
            if (ks.isEsc) {
                state.mode = AppMode::Utilities;
                disp.clearContent();
            }
        }
        break;

    case AppMode::Utilities:
        utilities->tick(state);
        if (utilities->backRequested()) {
            state.mode = AppMode::Translator;
            disp.clearContent();
        } else if (utilities->wifiSetupRequested()) {
            delete wifiSetup;
            wifiSetup  = new WifiSetupMode(disp, kb, wifiMgr, nvs);
            state.mode = AppMode::WifiSetup;
            disp.clearContent();
        }
        break;
    }

    delay(10);
}
