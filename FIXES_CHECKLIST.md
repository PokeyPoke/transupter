# ESP32 Cardputer - Critical Fixes Implementation Checklist

## 🔴 CRITICAL FIXES (MUST DO FIRST)

### [ ] Issue #1-2: HTTP Client Socket & Timeout Fixes
**File:** `src/api/http_client.cpp`  
**Changes:**
- [ ] Replace `http_client.cpp` with `http_client_FIXED.cpp` content
- [ ] Change line 215: `client.setTimeout(15)` → `client.setTimeout(HTTP_TIMEOUT_MS)` (15000ms)
- [ ] Add socket cleanup: `client.stop(); client.flush();` after all HTTP operations
- [ ] Add timeout constant: `static constexpr int HTTP_TIMEOUT_MS = 15000;`
- [ ] Fix chunk size validation in `readBodyBinaryChunked()` (prevent integer overflow)
- [ ] Pre-allocate String buffers in `readBodyPlain()` and `readBodyChunked()`

**Test After:**
- [ ] Make 10 API calls in sequence without WiFi timeout
- [ ] Monitor socket count stays ≤10 (not climbing)

---

### [ ] Issue #3: Watchdog Timer Support
**File:** `src/main.cpp`  
**Changes:**
- [ ] Add include: `#include <esp_task_wdt.h>`
- [ ] In `setup()` after boot: Add watchdog init
```cpp
esp_task_wdt_init(30, true);
esp_task_wdt_add(NULL);
```
- [ ] In `loop()` at START: Add `esp_task_wdt_reset();`
- [ ] In `bootWifi()` loop: Add `esp_task_wdt_reset();` after wifiMgr.tick()
- [ ] In `syncNtp()` loop: Add `esp_task_wdt_reset();` in while loop

**Test After:**
- [ ] Serial output shows "Watchdog timer initialized"
- [ ] Device doesn't hang during long operations

---

### [ ] Issue #4: Replace Blocking delay() in WiFi Boot
**File:** `src/main.cpp:74-86`  
**Changes:**
- [ ] In `bootWifi()`: Replace `delay(200)` with non-blocking pattern:
```cpp
// Option A: Use yield() + rate limiting
static unsigned long lastDrawMs = 0;
if (millis() - lastDrawMs >= 500) {
    lastDrawMs = millis();
    // ... update display
}
yield();
```

**Test After:**
- [ ] WiFi connects in 2-3 seconds (not extended timeouts)
- [ ] Boot countdown displays smoothly

---

### [ ] Issue #5: Chunked Response Parser - Bounds Checking
**File:** `src/api/http_client.cpp:62-82`  
**Changes:**
- [ ] Add chunk size validation:
```cpp
char* endPtr = nullptr;
long chunkSize = strtol(sizeLine.c_str(), &endPtr, 16);
if (endPtr == sizeLine.c_str() || chunkSize < 0 || chunkSize > 1000000) {
    Serial.printf("[HTTP] Invalid chunk size, aborting\n");
    break;
}
```
- [ ] Use `size_t` for buffer math (not `int`)
- [ ] Check bounds: `size_t toRead = min(remaining, maxLen - len);`

**Test After:**
- [ ] Download large API responses (>1KB) without buffer overflows
- [ ] Malformed responses handled gracefully

---

## 🟠 HIGH-PRIORITY FIXES (THIS WEEK)

### [ ] Issue #7: Recorder Memory Leak Fix
**Files:** `src/audio/recorder.h`, `src/audio/recorder.cpp`  
**Changes - Option 1 (Recommended):**
```cpp
// In recorder.h, add destructor
class Recorder {
public:
    Recorder() { }
    ~Recorder() {
        if (_buf) {
            free(_buf);
            _buf = nullptr;
        }
    }
private:
    // ... existing members
};
```

**Changes - Option 2 (Modern C++):**
```cpp
#include <memory>
class Recorder {
private:
    std::unique_ptr<int16_t[]> _buf;
};
```

**Test After:**
- [ ] Record 20+ times in sequence
- [ ] Check heap trend: `Serial.printf("Heap: %u\n", ESP.getFreeHeap());`
- [ ] Heap should not drop below ~50KB

---

### [ ] Issue #8: Player Error Handling
**File:** `src/audio/player.h`, `src/audio/player.cpp`  
**Changes:**
- [ ] Change return type to `bool`:
```cpp
bool Player::playWav(const uint8_t* data, size_t length) {
    if (!data || length < 44) return false;  // Min WAV header
    // ... play ...
    return true;
}
```
- [ ] Add WAV header validation
- [ ] Update translator.cpp to check return value

**Test After:**
- [ ] Invalid WAV data doesn't crash
- [ ] Error message shown to user

---

### [ ] Issue #6: Socket Leak in postJsonBinary()
**File:** `src/api/http_client.cpp:227-254`  
**Changes:**
- [ ] Add explicit cleanup:
```cpp
// After all reads
client.stop();
client.flush();
```

**Test After:**
- [ ] TTS doesn't accumulate open sockets

---

### [ ] Issue #9: Replace delay(5) in Recording Loop
**File:** `src/modes/translator.cpp:83`  
**Changes:**
```cpp
} else {
    yield();  // ← Much better
}
```

**Test After:**
- [ ] Recording completes in expected time
- [ ] WiFi doesn't timeout during recording

---

### [ ] Issue #10: HTTP Response Buffer Too Small
**File:** `src/api/http_client.cpp`  
**Changes:**
- [ ] Increase pre-allocation:
```cpp
String body;
body.reserve(8192);  // ← Was 512
```
- [ ] Increase read chunk size:
```cpp
uint8_t tmp[256];  // ← Was 128
```

**Test After:**
- [ ] Large responses (>1KB) handled without fragmentation
- [ ] TLS connection stability improves

---

### [ ] Issue #12: NTP Sync Non-Blocking
**File:** `src/main.cpp:95-100`  
**Changes:**
```cpp
while (!getLocalTime(&ti) && millis() - start < 2000) {
    esp_task_wdt_reset();  // Add watchdog reset
    yield();  // ← Replace delay(100)
    delayMicroseconds(10);
}
```

**Test After:**
- [ ] Boot doesn't hang on slow networks
- [ ] NTP still syncs when available

---

## 🟡 MEDIUM-PRIORITY FIXES (NEXT SPRINT)

### [ ] Issue #11: Event-Based Error Display (Non-Blocking)
**Files:** `src/app.h`, `src/modes/translator.cpp`, `src/main.cpp`  
**Changes:**
1. Add to AppState:
```cpp
struct AppState {
    // ... existing ...
    unsigned long popupDismissMs = 0;  // Dismiss popup at this time
};
```

2. In translator.cpp, replace delays with timers:
```cpp
if (captured == 0) {
    free(wav);
    disp.showPopup("Error", "Nothing recorded");
    state.popupDismissMs = millis() + 1500;
    _step = Step::Idle;
    return;  // Non-blocking!
}
```

3. In main.cpp loop, check for dismissal:
```cpp
if (state.popupDismissMs > 0 && millis() > state.popupDismissMs) {
    state.popupDismissMs = 0;
    disp.clearPopup();
}
```

**Test After:**
- [ ] Errors shown for correct duration
- [ ] Device responsive during error display
- [ ] Buttons work while popup shown

---

### [ ] Issue #13: Shorter WiFi Boot Timeout on Retry
**File:** `src/main.cpp`  
**Changes:**
```cpp
int timeout = (state.wifiConnected) ? 3000 : 5000;  // Faster retry
while (millis() - start < timeout) {
    // ...
}
```

**Test After:**
- [ ] First boot: 5s WiFi timeout
- [ ] Subsequent boots: 3s WiFi timeout

---

### [ ] Issue #14: Aggressive Battery Monitoring
**File:** `src/main.cpp`  
**Changes:**
```cpp
static constexpr int BATTERY_POLL_MS = 5000;  // ← Was 30000
if (millis() - lastBatteryMs > BATTERY_POLL_MS) {
    // ... check battery ...
    
    // Add critical shutdown
    if (state.batteryPct <= 5 && !state.criticalShown) {
        disp.showPopup("CRITICAL", "Shutting down...");
        delay(1000);
        esp_deep_sleep(0);  // Sleep until charger plugged in
    }
}
```

**Test After:**
- [ ] Battery updates every 5 seconds
- [ ] Low battery warning shown
- [ ] Critical battery triggers shutdown

---

### [ ] Issue #15: String Buffer Pre-Allocation
**File:** `src/api/anthropic.cpp`  
**Changes:**
```cpp
String prompt;
prompt.reserve(sourceText.length() + 128);  // Pre-allocate
prompt += "Translate...";
prompt += targetLanguage;
// ... etc
```

**Test After:**
- [ ] Large text (>500 chars) translates without issues
- [ ] No heap fragmentation during Anthropic calls

---

### [ ] Issue #16: Smart Retry Logic
**File:** `src/api/groq.cpp`  
**Changes:**
```cpp
// Distinguish transient vs permanent errors
bool isTransient = (resp.statusCode >= 500) ||  // Server error
                  (resp.statusCode == 429) ||   // Rate limit
                  (resp.statusCode == -1);      // Network error

if (!isTransient) {
    // Auth/not-found: fail immediately
    return error;
}

// Only transient errors retry with backoff
if (attempt < 2) {
    unsigned long backoff = 1000 * (1 << attempt);  // 1s, 2s
    delay(backoff);
}
```

**Test After:**
- [ ] Invalid API key fails in ~1 second (not 5)
- [ ] Network errors retry appropriately

---

## Final Verification

- [ ] All 17 issues reviewed
- [ ] Code compiles without errors
- [ ] No new compiler warnings
- [ ] All critical fixes implemented
- [ ] Testing checklist passed
- [ ] Commit to git: `fix: critical stability and memory issues`

## Files to Modify

**CRITICAL (Do First):**
1. `src/api/http_client.cpp` — Socket leaks, timeout, chunked parser
2. `src/main.cpp` — Watchdog, blocking delays, battery

**HIGH (This Week):**
3. `src/audio/recorder.h/cpp` — Memory leak
4. `src/audio/player.h/cpp` — Error handling
5. `src/modes/translator.cpp` — Replace delay(), event-based errors

**MEDIUM (Next Sprint):**
6. `src/api/groq.cpp` — Smart retry logic
7. `src/api/anthropic.cpp` — String pre-allocation
8. `src/app.h` — Add popup timer field

## Reference Files Provided

- `AUDIT_REPORT.md` — Full detailed analysis
- `http_client_FIXED.cpp` — Reference implementation
- `main_FIXED.cpp` — Reference implementation
- `FIXES_CHECKLIST.md` — This file
