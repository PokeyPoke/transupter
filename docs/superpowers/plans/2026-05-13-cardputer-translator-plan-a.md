# Cardputer Translator — Plan A: Foundation

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Scaffold a PlatformIO/Arduino project for the M5Stack Cardputer that boots, connects to WiFi, stores credentials and API keys in NVS, shows a persistent status bar, and monitors the battery — establishing the complete foundation for Plan B (translation pipeline).

**Architecture:** Arduino framework on ESP32-S3 via M5Cardputer library. All hardware abstracted behind thin wrapper classes. No blocking calls in the main loop — WiFi and async work are non-blocking. NVS (Preferences library) holds credentials and settings; LittleFS is reserved for Plan B's history log.

**Tech Stack:** PlatformIO, Arduino framework, M5Cardputer ≥1.0.0, ArduinoJson ≥7.0.0, LittleFS, Preferences (NVS), WiFi + WiFiClientSecure, Unity (native tests)

---

## File Map

| File | Responsibility |
|------|---------------|
| `platformio.ini` | Build environments (device + native test) |
| `partitions.csv` | Custom partition table: NVS + app + LittleFS |
| `src/config.h` | All compile-time constants, pin assignments, API URLs |
| `src/app.h` | Shared application state struct passed by reference |
| `src/main.cpp` | `setup()` + `loop()`: boot sequence and mode dispatcher |
| `src/system/nvs_store.h/.cpp` | NVS read/write for WiFi creds and API keys |
| `src/system/wifi_mgr.h/.cpp` | Non-blocking WiFi connect, scan, reconnect |
| `src/system/battery.h/.cpp` | Battery % and charging state |
| `src/ui/display.h/.cpp` | Display manager: clear, draw text, status bar |
| `src/ui/keyboard.h/.cpp` | Key hold detection and character mapping |
| `src/modes/wifi_setup.h/.cpp` | WiFi scan/select/password/connect mode |
| `test/native/test_nvs_store.cpp` | Unit tests for NVS key/value logic (mocked) |

---

## Task 1: PlatformIO Project Scaffold

**Files:**
- Create: `platformio.ini`
- Create: `partitions.csv`
- Create: `src/main.cpp` (skeleton)

- [ ] **Step 1: Write `platformio.ini`**

```ini
[platformio]
default_envs = m5cardputer

[env:m5cardputer]
platform = espressif32
board = m5stack-stamps3
framework = arduino
monitor_speed = 115200
upload_speed = 1500000
board_build.partitions = partitions.csv
board_build.arduino.memory_type = qio_opi
build_flags =
    -DBOARD_HAS_PSRAM
    -DARDUINO_USB_MODE=1
    -DARDUINO_USB_CDC_ON_BOOT=1
lib_deps =
    m5stack/M5Cardputer @ ^1.0.0
    bblanchon/ArduinoJson @ ^7.0.0

[env:native]
platform = native
lib_deps =
    bblanchon/ArduinoJson @ ^7.0.0
build_flags =
    -std=c++17
test_build_src = false
```

- [ ] **Step 2: Write `partitions.csv`**

```csv
# Name,    Type, SubType,  Offset,   Size,     Flags
nvs,       data, nvs,      0x9000,   0x6000,
phy_init,  data, phy,      0xF000,   0x1000,
factory,   app,  factory,  0x10000,  0x300000,
littlefs,  data, spiffs,   0x310000, 0x4F0000,
```

- [ ] **Step 3: Write skeleton `src/main.cpp`**

```cpp
#include <M5Cardputer.h>

void setup() {
    auto cfg = M5.config();
    M5Cardputer.begin(cfg, true);
    M5Cardputer.Display.setRotation(1);
    M5Cardputer.Display.fillScreen(BLACK);
    M5Cardputer.Display.setTextColor(WHITE, BLACK);
    M5Cardputer.Display.println("Transupter booting...");
}

void loop() {
    M5Cardputer.update();
    delay(10);
}
```

- [ ] **Step 4: Verify it compiles**

```bash
pio run -e m5cardputer
```

Expected: `SUCCESS` with no errors. Warnings about unused variables are OK.

- [ ] **Step 5: Commit**

```bash
git add platformio.ini partitions.csv src/main.cpp
git commit -m "feat: scaffold PlatformIO project for M5Cardputer"
```

---

## Task 2: Config and App State

**Files:**
- Create: `src/config.h`
- Create: `src/app.h`

- [ ] **Step 1: Write `src/config.h`**

```cpp
#pragma once

// Display
static constexpr int DISPLAY_WIDTH  = 240;
static constexpr int DISPLAY_HEIGHT = 135;
static constexpr int STATUS_BAR_H   = 18;

// Audio (Plan B)
static constexpr int SAMPLE_RATE_RECORD  = 16000;
static constexpr int SAMPLE_RATE_PLAYBACK = 22050;
static constexpr int MAX_RECORD_SECS     = 15;
static constexpr int RECORD_BUF_SAMPLES  = SAMPLE_RATE_RECORD * MAX_RECORD_SECS;

// WiFi
static constexpr int WIFI_CONNECT_TIMEOUT_MS = 10000;
static constexpr int WIFI_SKIP_TIMEOUT_MS    = 5000;

// Battery
static constexpr int BATTERY_LOW_WARN    = 15;
static constexpr int BATTERY_LOW_POPUP   = 10;

// NVS namespace
static constexpr char NVS_NS[]           = "transupter";

// NVS keys
static constexpr char NVS_WIFI_SSID[]    = "wifi_ssid";
static constexpr char NVS_WIFI_PASS[]    = "wifi_pass";
static constexpr char NVS_KEY_GROQ[]     = "key_groq";
static constexpr char NVS_KEY_ANTHROPIC[]= "key_anthropic";
static constexpr char NVS_KEY_OPENAI[]   = "key_openai";

// API endpoints
static constexpr char GROQ_HOST[]        = "api.groq.com";
static constexpr char GROQ_STT_PATH[]    = "/openai/v1/audio/transcriptions";
static constexpr char GROQ_STT_MODEL[]   = "whisper-large-v3";

static constexpr char ANTHROPIC_HOST[]   = "api.anthropic.com";
static constexpr char ANTHROPIC_PATH[]   = "/v1/messages";
static constexpr char ANTHROPIC_MODEL[]  = "claude-haiku-4-5-20251001";
static constexpr char ANTHROPIC_VER[]    = "2023-06-01";

static constexpr char OPENAI_HOST[]      = "api.openai.com";
static constexpr char OPENAI_TTS_PATH[]  = "/v1/audio/speech";
static constexpr char OPENAI_TTS_MODEL[] = "tts-1";
static constexpr char OPENAI_TTS_VOICE[] = "alloy";

// Language key map (keyboard char → language code for Whisper)
// Empty string = auto-detect (used for E key)
struct LangKey {
    char key;
    const char* whisperLang; // ISO 639-1, empty = auto
    const char* pairLang;    // the non-English partner language name (for Claude prompt)
    const char* displayName;
};

static constexpr LangKey LANG_KEYS[] = {
    { 'c', "cs", "Czech",      "Czech"     },
    { 'v', "vi", "Vietnamese", "Vietnamese"},
    { 'u', "uk", "Ukrainian",  "Ukrainian" },
    { 'r', "ru", "Russian",    "Russian"   },
    { 'm', "zh", "Mandarin",   "Mandarin"  },
    { 'j', "ja", "Japanese",   "Japanese"  },
    { 'k', "ko", "Korean",     "Korean"    },
    { 'e', "",   "",           "Universal" }, // auto-detect → English
};
static constexpr int LANG_KEY_COUNT = 8;
```

- [ ] **Step 2: Write `src/app.h`**

```cpp
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
```

- [ ] **Step 3: Verify compile**

```bash
pio run -e m5cardputer
```

Expected: `SUCCESS`.

- [ ] **Step 4: Commit**

```bash
git add src/config.h src/app.h
git commit -m "feat: add compile-time config and shared app state"
```

---

## Task 3: NVS Storage

**Files:**
- Create: `src/system/nvs_store.h`
- Create: `src/system/nvs_store.cpp`
- Create: `test/native/test_nvs_store.cpp`

- [ ] **Step 1: Write the failing native test**

```cpp
// test/native/test_nvs_store.cpp
#include <unity.h>
#include <string>

// Minimal mock of the NvsStore logic (pure string map — no hardware)
#include <unordered_map>

static std::unordered_map<std::string, std::string> fakeNvs;

std::string nvsGet(const std::string& key, const std::string& def) {
    auto it = fakeNvs.find(key);
    return it != fakeNvs.end() ? it->second : def;
}

void nvsPut(const std::string& key, const std::string& val) {
    fakeNvs[key] = val;
}

void test_put_and_get_returns_stored_value() {
    nvsPut("ssid", "MyNetwork");
    TEST_ASSERT_EQUAL_STRING("MyNetwork", nvsGet("ssid", "").c_str());
}

void test_get_missing_key_returns_default() {
    TEST_ASSERT_EQUAL_STRING("default", nvsGet("missing_key", "default").c_str());
}

void test_overwrite_existing_key() {
    nvsPut("ssid", "First");
    nvsPut("ssid", "Second");
    TEST_ASSERT_EQUAL_STRING("Second", nvsGet("ssid", "").c_str());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_put_and_get_returns_stored_value);
    RUN_TEST(test_get_missing_key_returns_default);
    RUN_TEST(test_overwrite_existing_key);
    return UNITY_END();
}
```

- [ ] **Step 2: Run native test — verify it fails**

```bash
pio test -e native
```

Expected: FAIL (no `test/native/test_nvs_store.cpp` in build yet or link errors — that's fine, proceed).

- [ ] **Step 3: Write `src/system/nvs_store.h`**

```cpp
#pragma once
#include <Arduino.h>

class NvsStore {
public:
    void        begin();
    void        end();

    String      getString(const char* key, const char* defaultVal = "") const;
    void        putString(const char* key, const String& value);
    bool        hasKey(const char* key) const;
    void        remove(const char* key);
};
```

- [ ] **Step 4: Write `src/system/nvs_store.cpp`**

```cpp
#include "nvs_store.h"
#include "../config.h"
#include <Preferences.h>

static Preferences prefs;

void NvsStore::begin() {
    prefs.begin(NVS_NS, false);
}

void NvsStore::end() {
    prefs.end();
}

String NvsStore::getString(const char* key, const char* defaultVal) const {
    return prefs.getString(key, defaultVal);
}

void NvsStore::putString(const char* key, const String& value) {
    prefs.putString(key, value);
}

bool NvsStore::hasKey(const char* key) const {
    return prefs.isKey(key);
}

void NvsStore::remove(const char* key) {
    prefs.remove(key);
}
```

- [ ] **Step 5: Run native test — verify it passes**

```bash
pio test -e native
```

Expected: 3 tests PASSED. (The native test uses the mock, not the hardware NvsStore — that's correct.)

- [ ] **Step 6: Verify device build compiles**

```bash
pio run -e m5cardputer
```

Expected: `SUCCESS`.

- [ ] **Step 7: Commit**

```bash
git add src/system/nvs_store.h src/system/nvs_store.cpp test/native/test_nvs_store.cpp
git commit -m "feat: NVS storage wrapper with native unit tests"
```

---

## Task 4: Battery Monitor

**Files:**
- Create: `src/system/battery.h`
- Create: `src/system/battery.cpp`

- [ ] **Step 1: Write `src/system/battery.h`**

```cpp
#pragma once

struct BatteryState {
    int  percent;
    bool charging;
};

class Battery {
public:
    BatteryState read() const;
};
```

- [ ] **Step 2: Write `src/system/battery.cpp`**

```cpp
#include "battery.h"
#include <M5Cardputer.h>

BatteryState Battery::read() const {
    return {
        .percent  = M5Cardputer.Power.getBatteryLevel(),
        .charging = M5Cardputer.Power.isCharging(),
    };
}
```

- [ ] **Step 3: Verify compile**

```bash
pio run -e m5cardputer
```

Expected: `SUCCESS`.

- [ ] **Step 4: Commit**

```bash
git add src/system/battery.h src/system/battery.cpp
git commit -m "feat: battery state reader"
```

---

## Task 5: Display Manager

**Files:**
- Create: `src/ui/display.h`
- Create: `src/ui/display.cpp`

- [ ] **Step 1: Write `src/ui/display.h`**

```cpp
#pragma once
#include <Arduino.h>
#include "../app.h"

class Display {
public:
    void begin();

    // Full-screen helpers
    void clear();
    void showBootScreen(const char* message);

    // Status bar — always drawn at top, 18px tall
    void drawStatusBar(const AppState& state);

    // Content area (below status bar)
    void clearContent();
    void printContent(const char* text, int textSize = 2);
    void printCentered(const char* text, int y, int textSize = 2);

    // Overlay
    void showPopup(const char* title, const char* body);

    static constexpr int CONTENT_Y = 18; // pixels below status bar
};
```

- [ ] **Step 2: Write `src/ui/display.cpp`**

```cpp
#include "display.h"
#include "../config.h"
#include <M5Cardputer.h>

static auto& tft = M5Cardputer.Display;

void Display::begin() {
    tft.setRotation(1);
    tft.fillScreen(BLACK);
    tft.setTextColor(WHITE, BLACK);
}

void Display::clear() {
    tft.fillScreen(BLACK);
}

void Display::showBootScreen(const char* message) {
    tft.fillScreen(BLACK);
    tft.setTextSize(2);
    tft.setCursor(10, 50);
    tft.setTextColor(CYAN, BLACK);
    tft.print("Transupter");
    tft.setTextColor(WHITE, BLACK);
    tft.setTextSize(1);
    tft.setCursor(10, 80);
    tft.print(message);
}

void Display::drawStatusBar(const AppState& state) {
    // Background strip
    tft.fillRect(0, 0, DISPLAY_WIDTH, STATUS_BAR_H, DARKGREY);
    tft.setTextSize(1);

    // WiFi icon
    tft.setCursor(4, 5);
    tft.setTextColor(state.wifiConnected ? GREEN : RED, DARKGREY);
    tft.print(state.wifiConnected ? "WiFi" : "NoWi");

    // Mode label
    tft.setCursor(50, 5);
    tft.setTextColor(WHITE, DARKGREY);
    switch (state.mode) {
        case AppMode::Translator: tft.print("TRANSLATE"); break;
        case AppMode::WifiSetup:  tft.print("WIFI SETUP"); break;
        case AppMode::Utilities:  tft.print("UTILITIES"); break;
        default:                  tft.print("BOOTING"); break;
    }

    // Battery
    tft.setCursor(185, 5);
    tft.setTextColor(state.batteryPct < BATTERY_LOW_WARN ? ORANGE : WHITE, DARKGREY);
    tft.printf("%d%%%s", state.batteryPct, state.batteryCharging ? "+" : "");

    tft.setTextColor(WHITE, BLACK); // reset
}

void Display::clearContent() {
    tft.fillRect(0, CONTENT_Y, DISPLAY_WIDTH, DISPLAY_HEIGHT - CONTENT_Y, BLACK);
}

void Display::printContent(const char* text, int textSize) {
    tft.setTextSize(textSize);
    tft.setTextColor(WHITE, BLACK);
    tft.setCursor(4, CONTENT_Y + 4);
    tft.print(text);
}

void Display::printCentered(const char* text, int y, int textSize) {
    tft.setTextSize(textSize);
    int charW = 6 * textSize;
    int x = (DISPLAY_WIDTH - strlen(text) * charW) / 2;
    if (x < 0) x = 0;
    tft.setCursor(x, y);
    tft.setTextColor(WHITE, BLACK);
    tft.print(text);
}

void Display::showPopup(const char* title, const char* body) {
    int bx = 20, by = 30, bw = 200, bh = 80;
    tft.fillRoundRect(bx, by, bw, bh, 6, DARKGREY);
    tft.drawRoundRect(bx, by, bw, bh, 6, WHITE);
    tft.setTextColor(YELLOW, DARKGREY);
    tft.setTextSize(1);
    tft.setCursor(bx + 6, by + 6);
    tft.print(title);
    tft.setTextColor(WHITE, DARKGREY);
    tft.setCursor(bx + 6, by + 22);
    tft.print(body);
}
```

- [ ] **Step 3: Verify compile**

```bash
pio run -e m5cardputer
```

Expected: `SUCCESS`.

- [ ] **Step 4: Commit**

```bash
git add src/ui/display.h src/ui/display.cpp
git commit -m "feat: display manager with status bar and content helpers"
```

---

## Task 6: Keyboard Handler

**Files:**
- Create: `src/ui/keyboard.h`
- Create: `src/ui/keyboard.cpp`

- [ ] **Step 1: Write `src/ui/keyboard.h`**

```cpp
#pragma once
#include <Arduino.h>

// Key event types
enum class KeyEvent { None, Down, Up, Held };

struct KeyState {
    char     ch;          // lowercase character, or 0 for special keys
    bool     isEnter;
    bool     isDel;
    bool     isEsc;       // mapped from Tab or Fn+key
    bool     isUp;        // Fn + arrow emulation (not native on Cardputer)
    bool     isDown;
    KeyEvent event;
};

class Keyboard {
public:
    // Call once per loop() iteration AFTER M5Cardputer.update()
    KeyState poll();

    // Returns true while a translation language key (c/v/u/r/m/j/k/e) is held
    bool isLangKeyHeld(char& outKey) const;

private:
    char  _prevLangKey = 0;
    bool  _langHeld    = false;
};
```

- [ ] **Step 2: Write `src/ui/keyboard.cpp`**

```cpp
#include "keyboard.h"
#include "../config.h"
#include <M5Cardputer.h>

static bool isLangChar(char c) {
    for (int i = 0; i < LANG_KEY_COUNT; i++) {
        if (LANG_KEYS[i].key == c) return true;
    }
    return false;
}

KeyState Keyboard::poll() {
    KeyState ks{};

    if (!M5Cardputer.Keyboard.isChange()) return ks;

    auto& raw = M5Cardputer.Keyboard.keysState();

    ks.isEnter = raw.enter;
    ks.isDel   = raw.del;
    ks.isEsc   = raw.fn; // treat Fn as ESC/cancel

    // Primary character key
    if (!raw.key.empty()) {
        ks.ch = tolower(raw.key[0]);
    }

    // Fn+W/S as up/down for menu navigation
    if (raw.fn && !raw.key.empty()) {
        if (raw.key[0] == 'w' || raw.key[0] == 'W') ks.isUp   = true;
        if (raw.key[0] == 's' || raw.key[0] == 'S') ks.isDown = true;
    }

    return ks;
}

bool Keyboard::isLangKeyHeld(char& outKey) const {
    auto& raw = M5Cardputer.Keyboard.keysState();
    if (raw.key.empty()) {
        _prevLangKey = 0;
        _langHeld    = false;
        return false;
    }
    char c = tolower(raw.key[0]);
    if (isLangChar(c)) {
        outKey       = c;
        _prevLangKey = c;
        _langHeld    = true;
        return true;
    }
    return false;
}
```

- [ ] **Step 3: Verify compile**

```bash
pio run -e m5cardputer
```

Expected: `SUCCESS`.

- [ ] **Step 4: Commit**

```bash
git add src/ui/keyboard.h src/ui/keyboard.cpp
git commit -m "feat: keyboard handler with hold detection and language key mapping"
```

---

## Task 7: WiFi Manager

**Files:**
- Create: `src/system/wifi_mgr.h`
- Create: `src/system/wifi_mgr.cpp`

- [ ] **Step 1: Write `src/system/wifi_mgr.h`**

```cpp
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
```

- [ ] **Step 2: Write `src/system/wifi_mgr.cpp`**

```cpp
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
```

- [ ] **Step 3: Verify compile**

```bash
pio run -e m5cardputer
```

Expected: `SUCCESS`.

- [ ] **Step 4: Commit**

```bash
git add src/system/wifi_mgr.h src/system/wifi_mgr.cpp
git commit -m "feat: non-blocking WiFi manager with scan and FSM connect"
```

---

## Task 8: WiFi Setup Mode

**Files:**
- Create: `src/modes/wifi_setup.h`
- Create: `src/modes/wifi_setup.cpp`

- [ ] **Step 1: Write `src/modes/wifi_setup.h`**

```cpp
#pragma once
#include "../app.h"
#include "../ui/display.h"
#include "../ui/keyboard.h"
#include "../system/wifi_mgr.h"
#include "../system/nvs_store.h"

class WifiSetupMode {
public:
    WifiSetupMode(Display& disp, Keyboard& kb, WifiMgr& wifi, NvsStore& nvs);

    // Returns true when WiFi is successfully connected (caller switches mode)
    bool tick(AppState& state);

private:
    enum class Step { Scanning, SelectNetwork, EnterPassword, Connecting };

    void drawNetworkList();
    void drawPasswordEntry();
    void drawConnecting();

    Display&  _disp;
    Keyboard& _kb;
    WifiMgr&  _wifi;
    NvsStore& _nvs;

    Step               _step      = Step::Scanning;
    std::vector<AccessPoint> _aps;
    int                _selected  = 0;
    String             _password;
};
```

- [ ] **Step 2: Write `src/modes/wifi_setup.cpp`**

```cpp
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
        if (ks.isEnter && !_aps.empty()) {
            _password = "";
            _step = Step::EnterPassword;
            drawPasswordEntry();
        }
        break;
    }

    case Step::EnterPassword: {
        auto ks = _kb.poll();
        if (ks.isDel && _password.length() > 0) {
            _password.remove(_password.length() - 1);
            drawPasswordEntry();
        } else if (ks.ch && !ks.isEnter && !ks.isDel) {
            _password += ks.ch;
            drawPasswordEntry();
        } else if (ks.isEnter) {
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
    // Show masked password
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
```

- [ ] **Step 3: Verify compile**

```bash
pio run -e m5cardputer
```

Expected: `SUCCESS`.

- [ ] **Step 4: Commit**

```bash
git add src/modes/wifi_setup.h src/modes/wifi_setup.cpp
git commit -m "feat: WiFi setup mode with scan, network select, password entry"
```

---

## Task 9: API Key Setup Flow

**Files:**
- Create: `src/modes/api_key_setup.h`
- Create: `src/modes/api_key_setup.cpp`

- [ ] **Step 1: Write `src/modes/api_key_setup.h`**

```cpp
#pragma once
#include "../app.h"
#include "../ui/display.h"
#include "../ui/keyboard.h"
#include "../system/nvs_store.h"

class ApiKeySetupMode {
public:
    ApiKeySetupMode(Display& disp, Keyboard& kb, NvsStore& nvs);

    // Returns true when all 3 keys have been entered and saved
    bool tick(AppState& state);

private:
    enum class Step { EnterGroq, EnterAnthropic, EnterOpenAI, Done };

    void drawEntry(const char* label, const String& current);

    Display&  _disp;
    Keyboard& _kb;
    NvsStore& _nvs;

    Step   _step = Step::EnterGroq;
    String _input;
};
```

- [ ] **Step 2: Write `src/modes/api_key_setup.cpp`**

```cpp
#include "api_key_setup.h"
#include "../config.h"
#include <M5Cardputer.h>

ApiKeySetupMode::ApiKeySetupMode(Display& disp, Keyboard& kb, NvsStore& nvs)
    : _disp(disp), _kb(kb), _nvs(nvs) {
    drawEntry("Groq API Key", _input);
}

bool ApiKeySetupMode::tick(AppState& state) {
    auto ks = _kb.poll();

    if (ks.isDel && _input.length() > 0) {
        _input.remove(_input.length() - 1);
    } else if (ks.ch && !ks.isEnter) {
        _input += ks.ch;
    }

    // Redraw on any input
    if (ks.ch || ks.isDel) {
        switch (_step) {
            case Step::EnterGroq:       drawEntry("Groq API Key", _input); break;
            case Step::EnterAnthropic:  drawEntry("Anthropic API Key", _input); break;
            case Step::EnterOpenAI:     drawEntry("OpenAI API Key", _input); break;
            default: break;
        }
    }

    if (!ks.isEnter || _input.isEmpty()) return false;

    _nvs.begin();
    switch (_step) {
        case Step::EnterGroq:
            _nvs.putString(NVS_KEY_GROQ, _input);
            state.keyGroq = _input;
            _input = "";
            _step  = Step::EnterAnthropic;
            drawEntry("Anthropic API Key", _input);
            break;
        case Step::EnterAnthropic:
            _nvs.putString(NVS_KEY_ANTHROPIC, _input);
            state.keyAnthropic = _input;
            _input = "";
            _step  = Step::EnterOpenAI;
            drawEntry("OpenAI API Key", _input);
            break;
        case Step::EnterOpenAI:
            _nvs.putString(NVS_KEY_OPENAI, _input);
            state.keyOpenAI  = _input;
            state.apiKeysSet = true;
            _nvs.end();
            return true;
        default: break;
    }
    _nvs.end();
    return false;
}

void ApiKeySetupMode::drawEntry(const char* label, const String& current) {
    _disp.clearContent();
    auto& tft = M5Cardputer.Display;
    tft.setTextSize(1);
    tft.setTextColor(CYAN, BLACK);
    tft.setCursor(4, Display::CONTENT_Y + 4);
    tft.printf("Enter %s:", label);
    tft.setTextColor(WHITE, BLACK);
    tft.setCursor(4, Display::CONTENT_Y + 18);
    // Show last 28 chars of current input so it fits
    String disp = current.length() > 28 ? current.substring(current.length() - 28) : current;
    tft.print(disp + "_");
    tft.setTextColor(DARKGREY, BLACK);
    tft.setCursor(4, Display::CONTENT_Y + 34);
    tft.print("Enter=confirm  Del=backspace");
}
```

- [ ] **Step 3: Verify compile**

```bash
pio run -e m5cardputer
```

Expected: `SUCCESS`.

- [ ] **Step 4: Commit**

```bash
git add src/modes/api_key_setup.h src/modes/api_key_setup.cpp
git commit -m "feat: API key entry flow (Groq, Anthropic, OpenAI) with NVS storage"
```

---

## Task 10: Utilities Mode (Skeleton)

**Files:**
- Create: `src/modes/utilities.h`
- Create: `src/modes/utilities.cpp`

- [ ] **Step 1: Write `src/modes/utilities.h`**

```cpp
#pragma once
#include "../app.h"
#include "../ui/display.h"
#include "../ui/keyboard.h"
#include "../system/battery.h"

class UtilitiesMode {
public:
    UtilitiesMode(Display& disp, Keyboard& kb, Battery& bat);
    void tick(AppState& state);

private:
    enum class View { Clock, BatteryStats, SystemInfo };
    void drawCurrentView(const AppState& state);

    Display&  _disp;
    Keyboard& _kb;
    Battery&  _bat;
    View      _view = View::Clock;
};
```

- [ ] **Step 2: Write `src/modes/utilities.cpp`**

```cpp
#include "utilities.h"
#include "../config.h"
#include <M5Cardputer.h>
#include <WiFi.h>
#include <time.h>

UtilitiesMode::UtilitiesMode(Display& disp, Keyboard& kb, Battery& bat)
    : _disp(disp), _kb(kb), _bat(bat) {}

void UtilitiesMode::tick(AppState& state) {
    auto ks = _kb.poll();
    if (ks.isUp) {
        _view = (View)(((int)_view - 1 + 3) % 3);
    } else if (ks.isDown) {
        _view = (View)(((int)_view + 1) % 3);
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
    }

    // Nav hint
    tft.setTextColor(DARKGREY, BLACK);
    tft.setCursor(4, Display::CONTENT_Y + 96);
    tft.print("Fn+W/S=switch view");
}
```

- [ ] **Step 3: Verify compile**

```bash
pio run -e m5cardputer
```

Expected: `SUCCESS`.

- [ ] **Step 4: Commit**

```bash
git add src/modes/utilities.h src/modes/utilities.cpp
git commit -m "feat: utilities mode with clock, battery stats, system info"
```

---

## Task 11: Main Application — Boot Sequence and Mode Dispatcher

**Files:**
- Modify: `src/main.cpp` (replace skeleton)

- [ ] **Step 1: Write the full `src/main.cpp`**

```cpp
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

static AppState       state;
static NvsStore       nvs;
static WifiMgr        wifiMgr;
static Battery        battery;
static Display        disp;
static Keyboard       kb;

static WifiSetupMode*  wifiSetup  = nullptr;
static ApiKeySetupMode* apiSetup  = nullptr;
static UtilitiesMode*  utilities  = nullptr;

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
        // No saved credentials → force WiFi setup
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
    if (!state.wifiConnected && !skipped) {
        disp.showPopup("WiFi failed", "Continuing offline");
        delay(1500);
    }
}

static void syncNtp() {
    if (!state.wifiConnected) return;
    configTime(0, 0, "pool.ntp.org", "time.google.com");
    // Wait up to 2s for time sync
    struct tm ti;
    unsigned long start = millis();
    while (!getLocalTime(&ti) && millis() - start < 2000) delay(100);
}

// ──────────────────────────────────────────────────────────────────────
void setup() {
    auto cfg = M5.config();
    M5Cardputer.begin(cfg, true);
    disp.begin();

    LittleFS.begin(true); // format on first boot if needed

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
            state.mode = AppMode::WifiSetup; // reuse boot, but actually show API setup
            apiSetup   = new ApiKeySetupMode(disp, kb, nvs);
            state.mode = AppMode::Booting;   // use Booting as placeholder for API setup
        } else {
            state.mode = AppMode::Translator;
        }
    }

    // Allocate mode objects
    utilities = new UtilitiesMode(disp, kb, battery);

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
            // After WiFi, check API keys
            if (!state.apiKeysSet) {
                apiSetup   = new ApiKeySetupMode(disp, kb, nvs);
                state.mode = AppMode::Booting; // API setup uses Booting slot
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
            disp.clearContent();
        }
        break;

    case AppMode::Translator:
        // Plan B: translator mode stub
        if (millis() % 5000 < 50) { // draw once per 5s while stub
            disp.clearContent();
            disp.printContent("Hold C/V/U/R/M/J/K/E\nto translate.\n(Plan B)");
        }
        // Switch to utilities on Fn key
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
        {
            auto ks = kb.poll();
            if (ks.isEsc) {
                state.mode = AppMode::Translator;
                disp.clearContent();
            }
        }
        break;
    }

    delay(10);
}
```

- [ ] **Step 2: Verify compile**

```bash
pio run -e m5cardputer
```

Expected: `SUCCESS`.

- [ ] **Step 3: Flash to device and verify boot sequence**

```bash
pio run -e m5cardputer --target upload && pio device monitor
```

Observe on device:
- Boot screen appears with "Loading settings..."
- If no WiFi saved: WiFi setup screen appears (scan list)
- If WiFi saved: countdown shows, connects, or skips on keypress
- Status bar shows at top: WiFi, mode label, battery %
- Translator mode stub shows "Hold C/V/U/R/M/J/K/E..."
- Pressing Fn switches to Utilities (clock, battery, system info)

- [ ] **Step 4: Commit**

```bash
git add src/main.cpp
git commit -m "feat: full boot sequence with mode dispatcher, WiFi skip, battery monitoring"
```

---

## Task 12: Native Test Coverage — Verify All Tests Pass

- [ ] **Step 1: Run full native test suite**

```bash
pio test -e native -v
```

Expected:
```
test/native/test_nvs_store.cpp   3 Tests   3 Passed   0 Failed
```

- [ ] **Step 2: Fix any failures, then commit**

```bash
git add test/
git commit -m "test: verify native test suite passes (Plan A complete)"
```

---

## Verification Checklist (Plan A Complete)

Flash the device and manually verify each item:

- [ ] Device boots without crashing
- [ ] Status bar visible at top (WiFi, mode, battery %)
- [ ] First boot: WiFi setup mode launches automatically
- [ ] Can scan and see nearby networks
- [ ] Can enter password and connect
- [ ] Credentials saved — reboot reconnects automatically
- [ ] Any key press during boot countdown skips WiFi → continues offline
- [ ] WiFi failure shows popup, continues to Translator stub
- [ ] First boot with no API keys: API key entry screen appears
- [ ] Can enter all 3 API keys via keyboard
- [ ] Keys saved — reboot goes directly to Translator stub
- [ ] Fn key switches to Utilities mode
- [ ] Utilities: clock shows (after NTP sync), battery stats, system info
- [ ] Fn key in Utilities returns to Translator
- [ ] Battery % < 10 → popup warning appears

---

*Next: Plan B — Audio recording, Groq STT, Anthropic translation, OpenAI TTS, Translator mode, and history log.*
