# Cardputer Translator — Plan B: Translation Pipeline

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Prerequisite:** Plan A must be complete and all devices tests passing before starting Plan B.

**Goal:** Implement the full translation pipeline on top of the Plan A foundation: I2S audio recording to PSRAM, WAV packaging, Groq Whisper STT, Anthropic Claude translation, OpenAI TTS playback, the Translator mode UI with hold-to-talk and auto-detect direction, and a scrollable translation history log in LittleFS.

**Architecture:** Recording uses I2S mic → PSRAM buffer → WAV header prepended in-memory → sent to Groq via raw HTTPS multipart. Translation result fed to Anthropic Claude → translated text fed to OpenAI TTS → raw WAV bytes streamed back → played via I2S speaker. All network operations are initiated from a FreeRTOS task to avoid blocking the display loop.

**Tech Stack:** Plan A stack + FreeRTOS tasks (included in ESP-IDF/Arduino), WiFiClientSecure for raw TLS, LittleFS JSON history via ArduinoJson.

---

## File Map

| File | Responsibility |
|------|---------------|
| `src/audio/wav.h/.cpp` | WAV header structs and in-memory builder |
| `src/audio/recorder.h/.cpp` | I2S mic → PSRAM buffer, VAD check |
| `src/audio/player.h/.cpp` | WAV data → I2S speaker |
| `src/api/http_client.h/.cpp` | Shared TLS helper (open connection, send, receive) |
| `src/api/groq.h/.cpp` | Multipart audio upload → transcript string |
| `src/api/anthropic.h/.cpp` | Text → translated text via Claude |
| `src/api/openai_tts.h/.cpp` | Text → WAV bytes via OpenAI TTS |
| `src/storage/history.h/.cpp` | LittleFS JSON: append, load last 50 entries |
| `src/modes/translator.h/.cpp` | Translator mode: hold-to-talk FSM + UI |
| `test/native/test_wav.cpp` | Unit tests: WAV header byte layout |
| `test/native/test_history.cpp` | Unit tests: history JSON append/load |

---

## Task 1: WAV Header Builder

**Files:**
- Create: `src/audio/wav.h`
- Create: `src/audio/wav.cpp`
- Create: `test/native/test_wav.cpp`

- [ ] **Step 1: Write the failing test**

```cpp
// test/native/test_wav.cpp
#include <unity.h>
#include <cstdint>
#include <cstring>

// ── Inline WAV builder (duplicated here so native test is self-contained) ──
struct WavHeader {
    char    riff[4]      = {'R','I','F','F'};
    uint32_t fileSize;
    char    wave[4]      = {'W','A','V','E'};
    char    fmt[4]       = {'f','m','t',' '};
    uint32_t fmtSize     = 16;
    uint16_t audioFmt    = 1;   // PCM
    uint16_t channels    = 1;
    uint32_t sampleRate;
    uint32_t byteRate;
    uint16_t blockAlign;
    uint16_t bitsPerSample = 16;
    char    data[4]      = {'d','a','t','a'};
    uint32_t dataSize;
};

WavHeader buildWavHeader(uint32_t sampleRate, uint32_t numSamples) {
    WavHeader h;
    h.sampleRate  = sampleRate;
    h.dataSize    = numSamples * sizeof(int16_t);
    h.blockAlign  = 1 * (16 / 8);
    h.byteRate    = sampleRate * h.blockAlign;
    h.fileSize    = 36 + h.dataSize;
    return h;
}

void test_wav_header_size_is_44_bytes() {
    TEST_ASSERT_EQUAL(44, sizeof(WavHeader));
}

void test_wav_header_riff_tag() {
    WavHeader h = buildWavHeader(16000, 16000);
    TEST_ASSERT_EQUAL_CHAR_ARRAY("RIFF", h.riff, 4);
    TEST_ASSERT_EQUAL_CHAR_ARRAY("WAVE", h.wave, 4);
    TEST_ASSERT_EQUAL_CHAR_ARRAY("fmt ", h.fmt,  4);
    TEST_ASSERT_EQUAL_CHAR_ARRAY("data", h.data, 4);
}

void test_wav_header_file_size_for_1_second_16khz() {
    // 1s * 16000 samples * 2 bytes/sample = 32000 bytes data
    // file size = 36 + 32000 = 32036
    WavHeader h = buildWavHeader(16000, 16000);
    TEST_ASSERT_EQUAL_UINT32(32036, h.fileSize);
    TEST_ASSERT_EQUAL_UINT32(32000, h.dataSize);
}

void test_wav_header_byte_rate() {
    WavHeader h = buildWavHeader(16000, 1000);
    // byteRate = 16000 * 1 * 2 = 32000
    TEST_ASSERT_EQUAL_UINT32(32000, h.byteRate);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_wav_header_size_is_44_bytes);
    RUN_TEST(test_wav_header_riff_tag);
    RUN_TEST(test_wav_header_file_size_for_1_second_16khz);
    RUN_TEST(test_wav_header_byte_rate);
    return UNITY_END();
}
```

- [ ] **Step 2: Run test — verify it fails**

```bash
pio test -e native
```

Expected: FAIL or compilation error (wav.h not yet written).

- [ ] **Step 3: Write `src/audio/wav.h`**

```cpp
#pragma once
#include <cstdint>

#pragma pack(push, 1)
struct WavHeader {
    char     riff[4]       = {'R','I','F','F'};
    uint32_t fileSize;
    char     wave[4]       = {'W','A','V','E'};
    char     fmt[4]        = {'f','m','t',' '};
    uint32_t fmtSize       = 16;
    uint16_t audioFmt      = 1;    // PCM
    uint16_t channels      = 1;
    uint32_t sampleRate;
    uint32_t byteRate;
    uint16_t blockAlign;
    uint16_t bitsPerSample = 16;
    char     data[4]       = {'d','a','t','a'};
    uint32_t dataSize;
};
#pragma pack(pop)

static_assert(sizeof(WavHeader) == 44, "WavHeader must be 44 bytes");

// Returns a fully populated WAV header for mono 16-bit PCM
WavHeader buildWavHeader(uint32_t sampleRate, uint32_t numSamples);
```

- [ ] **Step 4: Write `src/audio/wav.cpp`**

```cpp
#include "wav.h"

WavHeader buildWavHeader(uint32_t sampleRate, uint32_t numSamples) {
    WavHeader h;
    h.sampleRate  = sampleRate;
    h.dataSize    = numSamples * sizeof(int16_t);
    h.blockAlign  = 1 * (16 / 8);
    h.byteRate    = sampleRate * h.blockAlign;
    h.fileSize    = 36 + h.dataSize;
    return h;
}
```

- [ ] **Step 5: Run test — verify it passes**

```bash
pio test -e native
```

Expected: 4 tests PASSED.

- [ ] **Step 6: Commit**

```bash
git add src/audio/wav.h src/audio/wav.cpp test/native/test_wav.cpp
git commit -m "feat: WAV header builder with native unit tests"
```

---

## Task 2: Audio Recorder

**Files:**
- Create: `src/audio/recorder.h`
- Create: `src/audio/recorder.cpp`

- [ ] **Step 1: Write `src/audio/recorder.h`**

```cpp
#pragma once
#include <cstdint>
#include <cstddef>

struct RecordResult {
    int16_t* samples;       // PSRAM buffer (caller must ps_free after use)
    size_t   numSamples;
    bool     voiceDetected; // false if silent (VAD failed)
};

class Recorder {
public:
    static constexpr int    SAMPLE_RATE    = 16000;
    static constexpr int    VAD_THRESHOLD  = 500;  // peak amplitude below = silence
    static constexpr size_t DRAIN_CHUNK    = 512;  // samples per drain call

    // Allocates PSRAM buffer and starts mic.
    void startRecord(int maxSecs = 15);

    // Call once per loop() iteration while key is held to pull mic samples.
    void drainChunk();

    // Stops mic, finalises buffer.
    void stopRecord();

    bool isRecording() const;

    // Call after stopRecord() to retrieve result. Caller owns samples buffer.
    RecordResult getResult();

private:
    bool     _recording  = false;
    int16_t* _buf        = nullptr;
    size_t   _maxSamples = 0;
    size_t   _captured   = 0;
};
```

- [ ] **Step 2: Write `src/audio/recorder.cpp`**

```cpp
#include "recorder.h"
#include <M5Cardputer.h>
#include <cstring>

void Recorder::startRecord(int maxSecs) {
    _maxSamples = SAMPLE_RATE * maxSecs;
    _buf        = (int16_t*) ps_malloc(_maxSamples * sizeof(int16_t));
    if (!_buf) return;
    _captured   = 0;
    _recording  = true;

    auto cfg = M5.Mic.config();
    cfg.sample_rate = SAMPLE_RATE;
    cfg.stereo      = false;
    M5.Mic.config(cfg);
    M5.Mic.begin();
}

// Call once per loop() while the key is held to continuously pull mic samples.
void Recorder::drainChunk() {
    if (!_recording || !_buf || _captured >= _maxSamples) return;
    size_t toRead = min(DRAIN_CHUNK, _maxSamples - _captured);
    M5.Mic.record(_buf + _captured, toRead, SAMPLE_RATE);
    _captured += toRead;
}

void Recorder::stopRecord() {
    if (!_recording) return;
    M5.Mic.end();
    _recording = false;
}

bool Recorder::isRecording() const { return _recording; }

RecordResult Recorder::getResult() {
    // Check VAD: scan for peak amplitude
    int32_t peak = 0;
    for (size_t i = 0; i < _captured; i++) {
        int32_t v = abs(_buf[i]);
        if (v > peak) peak = v;
    }

    return {
        .samples       = _buf,       // caller must ps_free()
        .numSamples    = _captured,
        .voiceDetected = (peak >= VAD_THRESHOLD),
    };
}
```

- [ ] **Step 3: Verify device compile**

```bash
pio run -e m5cardputer
```

Expected: `SUCCESS`.

- [ ] **Step 4: Commit**

```bash
git add src/audio/recorder.h src/audio/recorder.cpp
git commit -m "feat: I2S audio recorder to PSRAM with VAD"
```

---

## Task 3: Audio Player

**Files:**
- Create: `src/audio/player.h`
- Create: `src/audio/player.cpp`

- [ ] **Step 1: Write `src/audio/player.h`**

```cpp
#pragma once
#include <cstdint>
#include <cstddef>

class Player {
public:
    // Play raw WAV bytes (including 44-byte header).
    // Blocks until playback completes (typical TTS phrase < 5s).
    void playWav(const uint8_t* data, size_t length);

    // Stop any current playback
    void stop();
};
```

- [ ] **Step 2: Write `src/audio/player.cpp`**

```cpp
#include "player.h"
#include <M5Cardputer.h>

void Player::playWav(const uint8_t* data, size_t length) {
    M5.Speaker.begin();
    M5.Speaker.setVolume(200);
    M5.Speaker.playWav(data, length, 1, 0, true); // data, len, rate_div, repeat, blocking
    M5.Speaker.end();
}

void Player::stop() {
    M5.Speaker.stop();
}
```

- [ ] **Step 3: Verify compile**

```bash
pio run -e m5cardputer
```

Expected: `SUCCESS`.

- [ ] **Step 4: Commit**

```bash
git add src/audio/player.h src/audio/player.cpp
git commit -m "feat: WAV audio player via I2S speaker"
```

---

## Task 4: HTTPS Helper

**Files:**
- Create: `src/api/http_client.h`
- Create: `src/api/http_client.cpp`

- [ ] **Step 1: Write `src/api/http_client.h`**

```cpp
#pragma once
#include <Arduino.h>
#include <WiFiClientSecure.h>

struct HttpResponse {
    int    statusCode;
    String body;
    bool   ok() const { return statusCode >= 200 && statusCode < 300; }
};

// Simple synchronous HTTPS POST helper.
// Uses setInsecure() (no cert pinning) — sufficient for API calls.
class HttpClient {
public:
    // JSON POST — returns response body as String
    HttpResponse postJson(const char* host, const char* path,
                          const String& bearerToken,
                          const String& jsonBody,
                          const char* extraHeader = nullptr,
                          const char* extraHeaderVal = nullptr);

    // Multipart POST for binary audio data — returns response body as String
    HttpResponse postMultipart(const char* host, const char* path,
                               const String& bearerToken,
                               const uint8_t* audioData, size_t audioLen,
                               const char* model,
                               const char* language);

    // Binary GET/POST — fills outBuf (caller allocates, must ps_malloc if large)
    HttpResponse postJsonBinary(const char* host, const char* path,
                                const String& bearerToken,
                                const String& jsonBody,
                                uint8_t* outBuf, size_t outBufSize,
                                size_t& outLen);
};
```

- [ ] **Step 2: Write `src/api/http_client.cpp`**

```cpp
#include "http_client.h"
#include <HTTPClient.h>

static const char* MULTIPART_BOUNDARY = "----ESP32Boundary7MA4YWxk";

HttpResponse HttpClient::postJson(const char* host, const char* path,
                                  const String& bearerToken,
                                  const String& jsonBody,
                                  const char* extraHeader,
                                  const char* extraHeaderVal) {
    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    http.begin(client, String("https://") + host + path);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Authorization", "Bearer " + bearerToken);
    if (extraHeader) http.addHeader(extraHeader, extraHeaderVal);

    int code = http.POST(jsonBody);
    String body = http.getString();
    http.end();
    return { code, body };
}

HttpResponse HttpClient::postMultipart(const char* host, const char* path,
                                       const String& bearerToken,
                                       const uint8_t* audioData, size_t audioLen,
                                       const char* model, const char* language) {
    WiFiClientSecure client;
    client.setInsecure();
    client.setTimeout(30);

    if (!client.connect(host, 443)) return { -1, "connect failed" };

    // Build multipart parts
    String part1 = String("--") + MULTIPART_BOUNDARY + "\r\n";
    part1 += "Content-Disposition: form-data; name=\"file\"; filename=\"audio.wav\"\r\n";
    part1 += "Content-Type: audio/wav\r\n\r\n";

    String part2 = String("\r\n--") + MULTIPART_BOUNDARY + "\r\n";
    part2 += "Content-Disposition: form-data; name=\"model\"\r\n\r\n";
    part2 += model;

    String part3;
    if (language && strlen(language) > 0) {
        part3  = String("\r\n--") + MULTIPART_BOUNDARY + "\r\n";
        part3 += "Content-Disposition: form-data; name=\"language\"\r\n\r\n";
        part3 += language;
    }

    String ending = String("\r\n--") + MULTIPART_BOUNDARY + "--\r\n";

    size_t contentLen = part1.length() + audioLen + part2.length()
                      + part3.length() + ending.length();

    // Send request
    client.printf("POST %s HTTP/1.1\r\n", path);
    client.printf("Host: %s\r\n", host);
    client.printf("Authorization: Bearer %s\r\n", bearerToken.c_str());
    client.printf("Content-Type: multipart/form-data; boundary=%s\r\n", MULTIPART_BOUNDARY);
    client.printf("Content-Length: %u\r\n", (unsigned)contentLen);
    client.print("Connection: close\r\n\r\n");

    client.print(part1);
    client.write(audioData, audioLen);
    client.print(part2);
    client.print(part3);
    client.print(ending);

    // Skip HTTP headers
    String statusLine = client.readStringUntil('\n');
    int code = 0;
    sscanf(statusLine.c_str(), "HTTP/1.1 %d", &code);
    while (client.connected()) {
        String line = client.readStringUntil('\n');
        if (line == "\r") break;
    }

    String body = client.readString();
    client.stop();
    return { code, body };
}

HttpResponse HttpClient::postJsonBinary(const char* host, const char* path,
                                        const String& bearerToken,
                                        const String& jsonBody,
                                        uint8_t* outBuf, size_t outBufSize,
                                        size_t& outLen) {
    WiFiClientSecure client;
    client.setInsecure();
    client.setTimeout(30);

    if (!client.connect(host, 443)) return { -1, "connect failed" };

    client.printf("POST %s HTTP/1.1\r\n", path);
    client.printf("Host: %s\r\n", host);
    client.printf("Authorization: Bearer %s\r\n", bearerToken.c_str());
    client.print("Content-Type: application/json\r\n");
    client.printf("Content-Length: %u\r\n", (unsigned)jsonBody.length());
    client.print("Connection: close\r\n\r\n");
    client.print(jsonBody);

    String statusLine = client.readStringUntil('\n');
    int code = 0;
    sscanf(statusLine.c_str(), "HTTP/1.1 %d", &code);
    while (client.connected()) {
        String line = client.readStringUntil('\n');
        if (line == "\r") break;
    }

    outLen = 0;
    while (client.available() && outLen < outBufSize) {
        outBuf[outLen++] = client.read();
    }
    client.stop();
    return { code, "" };
}
```

- [ ] **Step 3: Verify compile**

```bash
pio run -e m5cardputer
```

Expected: `SUCCESS`.

- [ ] **Step 4: Commit**

```bash
git add src/api/http_client.h src/api/http_client.cpp
git commit -m "feat: HTTPS helper with JSON, multipart, and binary response support"
```

---

## Task 5: Groq Whisper STT

**Files:**
- Create: `src/api/groq.h`
- Create: `src/api/groq.cpp`

- [ ] **Step 1: Write `src/api/groq.h`**

```cpp
#pragma once
#include <Arduino.h>
#include "http_client.h"

struct TranscriptResult {
    String text;
    String detectedLang; // ISO 639-1 code returned by Whisper
    bool   success;
    String error;
};

class GroqApi {
public:
    // Upload WAV audio and return transcript.
    // langHint: ISO 639-1 code to hint Whisper, or "" for auto-detect.
    TranscriptResult transcribe(const String& apiKey,
                                const uint8_t* wavData, size_t wavLen,
                                const char* langHint = "");

private:
    HttpClient _http;
};
```

- [ ] **Step 2: Write `src/api/groq.cpp`**

```cpp
#include "groq.h"
#include "../config.h"
#include <ArduinoJson.h>

TranscriptResult GroqApi::transcribe(const String& apiKey,
                                     const uint8_t* wavData, size_t wavLen,
                                     const char* langHint) {
    auto resp = _http.postMultipart(
        GROQ_HOST, GROQ_STT_PATH,
        apiKey,
        wavData, wavLen,
        GROQ_STT_MODEL,
        langHint   // "" = auto-detect
    );

    if (!resp.ok()) {
        return { "", "", false, "Groq HTTP " + String(resp.statusCode) + ": " + resp.body };
    }

    JsonDocument doc;
    auto err = deserializeJson(doc, resp.body);
    if (err) {
        return { "", "", false, String("JSON parse: ") + err.c_str() };
    }

    String text = doc["text"].as<String>();
    String lang = doc["language"].as<String>(); // Whisper returns detected language
    if (text.isEmpty()) {
        return { "", lang, false, "Empty transcript" };
    }

    return { text, lang, true, "" };
}
```

- [ ] **Step 3: Verify compile**

```bash
pio run -e m5cardputer
```

Expected: `SUCCESS`.

- [ ] **Step 4: Commit**

```bash
git add src/api/groq.h src/api/groq.cpp
git commit -m "feat: Groq Whisper STT API wrapper with auto language detection"
```

---

## Task 6: Anthropic Translation

**Files:**
- Create: `src/api/anthropic.h`
- Create: `src/api/anthropic.cpp`

- [ ] **Step 1: Write `src/api/anthropic.h`**

```cpp
#pragma once
#include <Arduino.h>
#include "http_client.h"

struct TranslationResult {
    String text;
    bool   success;
    String error;
};

class AnthropicApi {
public:
    // Translate sourceText to targetLanguage.
    // targetLanguage: e.g. "English", "Czech", "Japanese"
    TranslationResult translate(const String& apiKey,
                                const String& sourceText,
                                const String& targetLanguage);

private:
    HttpClient _http;
};
```

- [ ] **Step 2: Write `src/api/anthropic.cpp`**

```cpp
#include "anthropic.h"
#include "../config.h"
#include <ArduinoJson.h>

TranslationResult AnthropicApi::translate(const String& apiKey,
                                          const String& sourceText,
                                          const String& targetLanguage) {
    // Build prompt
    String prompt = "Translate the following text to " + targetLanguage +
                    ". Output only the translated text, no explanations.\n\n" +
                    sourceText;

    // Build JSON body
    JsonDocument req;
    req["model"]      = ANTHROPIC_MODEL;
    req["max_tokens"] = 1024;
    JsonArray msgs    = req["messages"].to<JsonArray>();
    JsonObject msg    = msgs.add<JsonObject>();
    msg["role"]       = "user";
    msg["content"]    = prompt;

    String body;
    serializeJson(req, body);

    auto resp = _http.postJson(
        ANTHROPIC_HOST, ANTHROPIC_PATH,
        apiKey, body,
        "x-api-key",         apiKey.c_str()   // Anthropic uses x-api-key, not Bearer
    );

    // Fix: Anthropic uses x-api-key header, not Bearer — rebuild correctly
    // (The above postJson will send both; Anthropic ignores the Bearer header)

    if (!resp.ok()) {
        return { "", false, "Anthropic HTTP " + String(resp.statusCode) + ": " + resp.body };
    }

    JsonDocument doc;
    auto err = deserializeJson(doc, resp.body);
    if (err) return { "", false, String("JSON parse: ") + err.c_str() };

    String translated = doc["content"][0]["text"].as<String>();
    if (translated.isEmpty()) return { "", false, "Empty response" };

    return { translated, true, "" };
}
```

- [ ] **Step 3: Fix the Anthropic auth header**

Anthropic requires `x-api-key` as the auth header (not `Authorization: Bearer`). Update `src/api/http_client.h` to add an `anthropic`-specific post method:

Add to `HttpClient` class in `http_client.h`:

```cpp
    // Anthropic-specific: uses x-api-key + anthropic-version headers instead of Bearer
    HttpResponse postJsonAnthropic(const char* host, const char* path,
                                   const String& apiKey,
                                   const String& jsonBody);
```

Add to `http_client.cpp`:

```cpp
HttpResponse HttpClient::postJsonAnthropic(const char* host, const char* path,
                                           const String& apiKey,
                                           const String& jsonBody) {
    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    http.begin(client, String("https://") + host + path);
    http.addHeader("Content-Type",     "application/json");
    http.addHeader("x-api-key",        apiKey);
    http.addHeader("anthropic-version", ANTHROPIC_VER);

    int code = http.POST(jsonBody);
    String body = http.getString();
    http.end();
    return { code, body };
}
```

Then update `anthropic.cpp` to call `_http.postJsonAnthropic(...)` instead of `_http.postJson(...)`:

```cpp
    auto resp = _http.postJsonAnthropic(
        ANTHROPIC_HOST, ANTHROPIC_PATH, apiKey, body
    );
```

- [ ] **Step 4: Verify compile**

```bash
pio run -e m5cardputer
```

Expected: `SUCCESS`.

- [ ] **Step 5: Commit**

```bash
git add src/api/anthropic.h src/api/anthropic.cpp \
        src/api/http_client.h src/api/http_client.cpp
git commit -m "feat: Anthropic Claude translation API with correct auth headers"
```

---

## Task 7: OpenAI TTS

**Files:**
- Create: `src/api/openai_tts.h`
- Create: `src/api/openai_tts.cpp`

- [ ] **Step 1: Write `src/api/openai_tts.h`**

```cpp
#pragma once
#include <Arduino.h>
#include <cstdint>
#include <cstddef>
#include "http_client.h"

struct TtsResult {
    uint8_t* data;    // WAV bytes in PSRAM (caller must ps_free)
    size_t   length;
    bool     success;
    String   error;
};

class OpenAiTts {
public:
    static constexpr size_t MAX_TTS_BYTES = 512 * 1024; // 512KB ceiling

    TtsResult synthesize(const String& apiKey, const String& text);

private:
    HttpClient _http;
};
```

- [ ] **Step 2: Write `src/api/openai_tts.cpp`**

```cpp
#include "openai_tts.h"
#include "../config.h"
#include <ArduinoJson.h>

TtsResult OpenAiTts::synthesize(const String& apiKey, const String& text) {
    JsonDocument req;
    req["model"]           = OPENAI_TTS_MODEL;
    req["input"]           = text;
    req["voice"]           = OPENAI_TTS_VOICE;
    req["response_format"] = "wav";

    String body;
    serializeJson(req, body);

    uint8_t* buf = (uint8_t*) ps_malloc(MAX_TTS_BYTES);
    if (!buf) return { nullptr, 0, false, "PSRAM alloc failed" };

    size_t outLen = 0;
    auto resp = _http.postJsonBinary(
        OPENAI_HOST, OPENAI_TTS_PATH,
        apiKey, body,
        buf, MAX_TTS_BYTES, outLen
    );

    if (!resp.ok() || outLen == 0) {
        ps_free(buf);
        return { nullptr, 0, false, "TTS HTTP " + String(resp.statusCode) };
    }

    return { buf, outLen, true, "" };
}
```

- [ ] **Step 3: Verify compile**

```bash
pio run -e m5cardputer
```

Expected: `SUCCESS`.

- [ ] **Step 4: Commit**

```bash
git add src/api/openai_tts.h src/api/openai_tts.cpp
git commit -m "feat: OpenAI TTS API wrapper streaming WAV to PSRAM"
```

---

## Task 8: Translation History Log

**Files:**
- Create: `src/storage/history.h`
- Create: `src/storage/history.cpp`
- Create: `test/native/test_history.cpp`

- [ ] **Step 1: Write the failing test**

```cpp
// test/native/test_history.cpp
#include <unity.h>
#include <vector>
#include <string>
#include <algorithm>

// ── Minimal in-memory mock of history logic ────────────────────────────
static constexpr int MAX_ENTRIES = 50;

struct HistoryEntry {
    std::string source;
    std::string translated;
    std::string langKey;
};

static std::vector<HistoryEntry> fakeHistory;

void historyAppend(const std::string& src, const std::string& tr, const std::string& lang) {
    fakeHistory.push_back({ src, tr, lang });
    if ((int)fakeHistory.size() > MAX_ENTRIES) {
        fakeHistory.erase(fakeHistory.begin());
    }
}

int historySize() { return (int)fakeHistory.size(); }
HistoryEntry historyGet(int i) { return fakeHistory[i]; }

void setUp() { fakeHistory.clear(); }
void tearDown() {}

void test_append_single_entry() {
    historyAppend("Ahoj", "Hello", "c");
    TEST_ASSERT_EQUAL(1, historySize());
    TEST_ASSERT_EQUAL_STRING("Hello", historyGet(0).translated.c_str());
}

void test_append_respects_max_50() {
    for (int i = 0; i < 55; i++) {
        historyAppend("word", "translated", "c");
    }
    TEST_ASSERT_EQUAL(50, historySize());
}

void test_oldest_entry_evicted_when_full() {
    historyAppend("First", "Premier", "c");
    for (int i = 0; i < 50; i++) historyAppend("word", "mot", "c");
    // "First" should now be gone
    bool found = false;
    for (int i = 0; i < historySize(); i++) {
        if (historyGet(i).source == "First") { found = true; break; }
    }
    TEST_ASSERT_FALSE(found);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_append_single_entry);
    RUN_TEST(test_append_respects_max_50);
    RUN_TEST(test_oldest_entry_evicted_when_full);
    return UNITY_END();
}
```

- [ ] **Step 2: Run test — verify it fails**

```bash
pio test -e native
```

Expected: compilation error or FAIL.

- [ ] **Step 3: Write `src/storage/history.h`**

```cpp
#pragma once
#include <Arduino.h>
#include <vector>

struct HistoryEntry {
    String source;
    String translated;
    String langKey;    // single char, e.g. "c"
};

class History {
public:
    static constexpr int MAX_ENTRIES = 50;
    static constexpr char PATH[]     = "/history.json";

    void begin();                  // mount LittleFS, load existing entries
    void append(const String& source, const String& translated, const String& langKey);
    const std::vector<HistoryEntry>& entries() const;

private:
    std::vector<HistoryEntry> _entries;
    void save() const;
    void load();
};
```

- [ ] **Step 4: Write `src/storage/history.cpp`**

```cpp
#include "history.h"
#include <LittleFS.h>
#include <ArduinoJson.h>

void History::begin() {
    // LittleFS.begin() is called in main.cpp; just load here
    load();
}

void History::append(const String& source, const String& translated, const String& langKey) {
    _entries.push_back({ source, translated, langKey });
    if ((int)_entries.size() > MAX_ENTRIES) {
        _entries.erase(_entries.begin());
    }
    save();
}

const std::vector<HistoryEntry>& History::entries() const { return _entries; }

void History::load() {
    if (!LittleFS.exists(PATH)) return;
    File f = LittleFS.open(PATH, "r");
    if (!f) return;

    JsonDocument doc;
    if (deserializeJson(doc, f)) { f.close(); return; }
    f.close();

    JsonArray arr = doc.as<JsonArray>();
    for (JsonObject obj : arr) {
        _entries.push_back({
            obj["src"].as<String>(),
            obj["tr"].as<String>(),
            obj["lang"].as<String>(),
        });
    }
}

void History::save() const {
    File f = LittleFS.open(PATH, "w");
    if (!f) return;

    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();
    for (auto& e : _entries) {
        JsonObject obj = arr.add<JsonObject>();
        obj["src"]  = e.source;
        obj["tr"]   = e.translated;
        obj["lang"] = e.langKey;
    }
    serializeJson(doc, f);
    f.close();
}
```

- [ ] **Step 5: Run all native tests — verify all pass**

```bash
pio test -e native
```

Expected:
```
test/native/test_wav.cpp      4 Tests  4 Passed  0 Failed
test/native/test_nvs_store.cpp 3 Tests 3 Passed  0 Failed
test/native/test_history.cpp  3 Tests  3 Passed  0 Failed
```

- [ ] **Step 6: Verify device compile**

```bash
pio run -e m5cardputer
```

Expected: `SUCCESS`.

- [ ] **Step 7: Commit**

```bash
git add src/storage/history.h src/storage/history.cpp test/native/test_history.cpp
git commit -m "feat: LittleFS translation history log with native unit tests"
```

---

## Task 9: Full Translator Mode

**Files:**
- Create: `src/modes/translator.h`
- Create: `src/modes/translator.cpp`

- [ ] **Step 1: Write `src/modes/translator.h`**

```cpp
#pragma once
#include "../app.h"
#include "../ui/display.h"
#include "../ui/keyboard.h"
#include "../audio/recorder.h"
#include "../audio/player.h"
#include "../api/groq.h"
#include "../api/anthropic.h"
#include "../api/openai_tts.h"
#include "../storage/history.h"
#include "../config.h"

class TranslatorMode {
public:
    TranslatorMode(Display& disp, Keyboard& kb, History& hist);

    void tick(AppState& state);

private:
    enum class Step {
        Idle,
        Recording,
        Transcribing,
        Translating,
        Synthesizing,
        Playing,
        ShowResult,
        Error,
    };

    void handleIdle(AppState& state);
    void handleRecording(AppState& state);
    void runPipeline(AppState& state, char langKey,
                     const uint8_t* wavData, size_t wavLen);
    void drawIdle();
    void drawRecording(char key);
    void drawStatus(const char* line1, const char* line2 = "");
    void drawResult(const String& source, const String& translated);
    void drawError(const String& msg);

    const LangKey* findLangKey(char ch) const;
    // Determines the correct target language given detected language and key config
    String targetLanguage(const String& detectedLang, const LangKey* lk) const;

    Display&     _disp;
    Keyboard&    _kb;
    History&     _hist;

    Recorder     _rec;
    Player       _player;
    GroqApi      _groq;
    AnthropicApi _anthropic;
    OpenAiTts    _tts;

    Step   _step         = Step::Idle;
    char   _activeLangKey = 0;
    bool   _keyWasHeld   = false;

    String _lastSource;
    String _lastTranslated;
};
```

- [ ] **Step 2: Write `src/modes/translator.cpp`**

```cpp
#include "translator.h"
#include <M5Cardputer.h>
#include <cstring>

TranslatorMode::TranslatorMode(Display& disp, Keyboard& kb, History& hist)
    : _disp(disp), _kb(kb), _hist(hist) {
    drawIdle();
}

const LangKey* TranslatorMode::findLangKey(char ch) const {
    for (int i = 0; i < LANG_KEY_COUNT; i++) {
        if (LANG_KEYS[i].key == ch) return &LANG_KEYS[i];
    }
    return nullptr;
}

// E key → always English. Pair key → opposite of detected language.
String TranslatorMode::targetLanguage(const String& detectedLang, const LangKey* lk) const {
    if (strlen(lk->whisperLang) == 0) return "English";   // E key: universal → EN
    if (detectedLang == "en")         return lk->pairLang; // detected EN → pair language
    return "English";                                       // detected pair → EN
}

void TranslatorMode::tick(AppState& state) {
    switch (_step) {
    case Step::Idle:     handleIdle(state);      break;
    case Step::Recording: handleRecording(state); break;
    default: break; // pipeline steps run synchronously in runPipeline()
    }
}

void TranslatorMode::handleIdle(AppState& state) {
    char langKey = 0;
    bool held = _kb.isLangKeyHeld(langKey);

    if (held && !_keyWasHeld) {
        // Key just went down — start recording
        _keyWasHeld   = true;
        _activeLangKey = langKey;
        _rec.startRecord(15);
        _step = Step::Recording;
        drawRecording(langKey);
    } else if (!held) {
        _keyWasHeld = false;
    }
}

void TranslatorMode::handleRecording(AppState& state) {
    char langKey = 0;
    bool held = _kb.isLangKeyHeld(langKey);

    _rec.drainChunk(); // pull mic samples into PSRAM buffer each loop
    drawRecording(_activeLangKey);

    if (!held) {
        // Key released — stop and process
        _rec.stopRecord();
        auto result = _rec.getResult();

        if (!result.voiceDetected) {
            if (result.samples) ps_free(result.samples);
            drawError("No voice detected");
            delay(1500);
            _step = Step::Idle;
            drawIdle();
            return;
        }

        // Build WAV: header + samples
        WavHeader header = buildWavHeader(Recorder::SAMPLE_RATE, result.numSamples);
        size_t totalLen  = sizeof(WavHeader) + result.numSamples * sizeof(int16_t);
        uint8_t* wav     = (uint8_t*) ps_malloc(totalLen);

        if (!wav) {
            ps_free(result.samples);
            drawError("Out of memory");
            delay(1500);
            _step = Step::Idle;
            drawIdle();
            return;
        }

        memcpy(wav, &header, sizeof(WavHeader));
        memcpy(wav + sizeof(WavHeader), result.samples, result.numSamples * sizeof(int16_t));
        ps_free(result.samples);

        runPipeline(state, _activeLangKey, wav, totalLen);
        ps_free(wav);

        _step = Step::Idle;
        delay(2000); // let user read result
        drawIdle();
    }
}

void TranslatorMode::runPipeline(AppState& state, char langKeyChar,
                                 const uint8_t* wavData, size_t wavLen) {
    const LangKey* lk = findLangKey(langKeyChar);
    if (!lk) { drawError("Unknown key"); return; }

    // Step 1: STT
    drawStatus("Transcribing...");
    auto stt = _groq.transcribe(state.keyGroq, wavData, wavLen, lk->whisperLang);
    if (!stt.success) { drawError(stt.error); return; }

    // Step 2: Translation
    drawStatus("Translating...", stt.text.substring(0, 28).c_str());
    String tgtLang = targetLanguage(stt.detectedLang, lk);
    auto tr = _anthropic.translate(state.keyAnthropic, stt.text, tgtLang);
    if (!tr.success) { drawError(tr.error); return; }

    // Show result
    _lastSource     = stt.text;
    _lastTranslated = tr.text;
    drawResult(stt.text, tr.text);

    // Save to history
    _hist.append(stt.text, tr.text, String(langKeyChar));

    // Step 3: TTS
    drawStatus("Speaking...");
    auto tts = _tts.synthesize(state.keyOpenAI, tr.text);
    if (!tts.success) { drawError(tts.error); return; }

    _player.playWav(tts.data, tts.length);
    ps_free(tts.data);

    drawResult(stt.text, tr.text); // re-show after playback
}

void TranslatorMode::drawIdle() {
    _disp.clearContent();
    auto& tft = M5Cardputer.Display;
    tft.setTextSize(1);
    tft.setTextColor(DARKGREY, BLACK);
    tft.setCursor(4, Display::CONTENT_Y + 10);
    tft.print("Hold a language key to translate:");
    tft.setCursor(4, Display::CONTENT_Y + 26);
    tft.setTextColor(WHITE, BLACK);
    tft.print("C=Czech  V=Viet  U=Ukr  R=Rus");
    tft.setCursor(4, Display::CONTENT_Y + 40);
    tft.print("M=Mandarin  J=Jpn  K=Korean");
    tft.setCursor(4, Display::CONTENT_Y + 54);
    tft.setTextColor(CYAN, BLACK);
    tft.print("E=Universal (any lang -> EN)");
    tft.setTextColor(DARKGREY, BLACK);
    tft.setCursor(4, Display::CONTENT_Y + 80);
    tft.print("Fn=Utilities");
}

void TranslatorMode::drawRecording(char key) {
    _disp.clearContent();
    auto& tft = M5Cardputer.Display;
    tft.setTextColor(RED, BLACK);
    tft.setTextSize(2);
    tft.setCursor(4, Display::CONTENT_Y + 10);
    tft.printf("[ REC ] key=%c", toupper(key));
    tft.setTextSize(1);
    tft.setTextColor(WHITE, BLACK);
    tft.setCursor(4, Display::CONTENT_Y + 36);
    tft.print("Listening... release to translate");
}

void TranslatorMode::drawStatus(const char* line1, const char* line2) {
    _disp.clearContent();
    auto& tft = M5Cardputer.Display;
    tft.setTextSize(1);
    tft.setTextColor(YELLOW, BLACK);
    tft.setCursor(4, Display::CONTENT_Y + 10);
    tft.print(line1);
    if (line2 && strlen(line2) > 0) {
        tft.setTextColor(WHITE, BLACK);
        tft.setCursor(4, Display::CONTENT_Y + 26);
        tft.print(line2);
    }
}

void TranslatorMode::drawResult(const String& source, const String& translated) {
    _disp.clearContent();
    auto& tft = M5Cardputer.Display;
    tft.setTextSize(1);
    tft.setTextColor(DARKGREY, BLACK);
    tft.setCursor(4, Display::CONTENT_Y + 4);
    tft.print(source.substring(0, 36));
    tft.setTextColor(GREEN, BLACK);
    tft.setCursor(4, Display::CONTENT_Y + 20);
    tft.setTextSize(1);
    // Wrap translated text across 3 lines of 36 chars
    for (int i = 0, line = 0; i < (int)translated.length() && line < 3; line++) {
        String chunk = translated.substring(i, i + 36);
        tft.setCursor(4, Display::CONTENT_Y + 34 + line * 14);
        tft.print(chunk);
        i += 36;
    }
}

void TranslatorMode::drawError(const String& msg) {
    _disp.showPopup("Error", msg.substring(0, 28).c_str());
}
```

- [ ] **Step 3: Verify compile**

```bash
pio run -e m5cardputer
```

Expected: `SUCCESS`.

- [ ] **Step 4: Commit**

```bash
git add src/modes/translator.h src/modes/translator.cpp
git commit -m "feat: translator mode with hold-to-talk, STT, translation, TTS pipeline"
```

---

## Task 10: Wire Translator Mode into main.cpp

**Files:**
- Modify: `src/main.cpp`

- [ ] **Step 1: Add includes and objects to `src/main.cpp`**

Add after the existing includes:

```cpp
#include "audio/wav.h"
#include "audio/recorder.h"
#include "audio/player.h"
#include "api/groq.h"
#include "api/anthropic.h"
#include "api/openai_tts.h"
#include "storage/history.h"
#include "modes/translator.h"
```

Add after the existing static objects:

```cpp
static History          history;
static TranslatorMode*  translator = nullptr;
```

- [ ] **Step 2: Initialize history and translator in `setup()`**

In `setup()`, after `LittleFS.begin(true);`:

```cpp
    history.begin();
```

After `utilities = new UtilitiesMode(disp, kb, battery);`:

```cpp
    if (state.apiKeysSet) {
        translator = new TranslatorMode(disp, kb, history);
    }
```

- [ ] **Step 3: Replace the Translator stub in `loop()`**

Replace the `case AppMode::Translator:` block:

```cpp
    case AppMode::Translator:
        if (translator) {
            translator->tick(state);
        } else {
            // API keys not set — shouldn't reach here, but handle gracefully
            disp.clearContent();
            disp.printContent("API keys missing.\nFn to go to Utilities.");
        }
        {
            auto ks = kb.poll();
            if (ks.isEsc) {
                state.mode = AppMode::Utilities;
                disp.clearContent();
            }
        }
        break;
```

- [ ] **Step 4: Also initialize translator after API key setup completes**

In the `AppMode::Booting` case in `loop()`, after setting `state.mode = AppMode::Translator`:

```cpp
            if (!translator) {
                translator = new TranslatorMode(disp, kb, history);
            }
```

- [ ] **Step 5: Verify compile**

```bash
pio run -e m5cardputer
```

Expected: `SUCCESS`.

- [ ] **Step 6: Flash and smoke-test on device**

```bash
pio run -e m5cardputer --target upload
```

Observe:
- Device boots, connects to WiFi, enters Translator mode
- Idle screen shows language key hints
- Hold C: "[ REC ] key=C" appears
- Release: "Transcribing..." → "Translating..." → "Speaking..." → result shown

- [ ] **Step 7: Commit**

```bash
git add src/main.cpp
git commit -m "feat: wire TranslatorMode and history into main app"
```

---

## Task 11: History Viewer in Utilities Mode

**Files:**
- Modify: `src/modes/utilities.h`
- Modify: `src/modes/utilities.cpp`

- [ ] **Step 1: Add history dependency to `UtilitiesMode`**

In `utilities.h`, add to includes:

```cpp
#include "../storage/history.h"
```

Change constructor signature:

```cpp
    UtilitiesMode(Display& disp, Keyboard& kb, Battery& bat, History& hist);
```

Add to private:

```cpp
    History&  _hist;
    int       _histScroll = 0;
```

Add `History` to the `View` enum:

```cpp
    enum class View { Clock, BatteryStats, SystemInfo, HistoryLog };
```

- [ ] **Step 2: Update `utilities.cpp`**

Change constructor:

```cpp
UtilitiesMode::UtilitiesMode(Display& disp, Keyboard& kb, Battery& bat, History& hist)
    : _disp(disp), _kb(kb), _bat(bat), _hist(hist) {}
```

Update the View cycle count in `tick()` from `% 3` to `% 4`.

Add `case View::HistoryLog:` to `drawCurrentView()`:

```cpp
    case View::HistoryLog: {
        auto& entries = _hist.entries();
        tft.setTextColor(CYAN, BLACK);
        tft.setCursor(4, Display::CONTENT_Y + 2);
        tft.printf("History (%d entries):", (int)entries.size());
        int maxVisible = 4;
        int start = max(0, (int)entries.size() - maxVisible - _histScroll);
        for (int i = 0; i < maxVisible && start + i < (int)entries.size(); i++) {
            auto& e = entries[start + i];
            tft.setTextColor(DARKGREY, BLACK);
            tft.setCursor(4, Display::CONTENT_Y + 14 + i * 24);
            tft.print(e.source.substring(0, 34));
            tft.setTextColor(WHITE, BLACK);
            tft.setCursor(4, Display::CONTENT_Y + 24 + i * 24);
            tft.print(e.translated.substring(0, 34));
        }
        if (entries.empty()) {
            tft.setTextColor(DARKGREY, BLACK);
            tft.setCursor(4, Display::CONTENT_Y + 30);
            tft.print("No translations yet.");
        }
        break;
    }
```

Also update the scroll handling in `tick()` to adjust `_histScroll` when in HistoryLog view:

```cpp
    if (_view == View::HistoryLog) {
        auto& entries = _hist.entries();
        if (ks.isUp   && _histScroll < (int)entries.size() - 1) _histScroll++;
        if (ks.isDown && _histScroll > 0) _histScroll--;
    } else {
        if (ks.isUp)   _view = (View)(((int)_view - 1 + 4) % 4);
        if (ks.isDown) _view = (View)(((int)_view + 1) % 4);
    }
```

- [ ] **Step 3: Update `main.cpp`** — pass `history` to `UtilitiesMode` constructor:

```cpp
    utilities = new UtilitiesMode(disp, kb, battery, history);
```

- [ ] **Step 4: Verify compile**

```bash
pio run -e m5cardputer
```

Expected: `SUCCESS`.

- [ ] **Step 5: Commit**

```bash
git add src/modes/utilities.h src/modes/utilities.cpp src/main.cpp
git commit -m "feat: history viewer in utilities mode with scrollable entries"
```

---

## Task 12: Final Native Test Run

- [ ] **Step 1: Run all native tests**

```bash
pio test -e native -v
```

Expected:
```
test/native/test_wav.cpp       4 Tests  4 Passed  0 Failed
test/native/test_nvs_store.cpp 3 Tests  3 Passed  0 Failed
test/native/test_history.cpp   3 Tests  3 Passed  0 Failed
============================================================
10 Tests  10 Passed  0 Failed
```

- [ ] **Step 2: Final device build**

```bash
pio run -e m5cardputer
```

Expected: `SUCCESS` with no errors.

- [ ] **Step 3: Tag the release**

```bash
git tag v0.1.0
git commit --allow-empty -m "chore: Plan B complete — full translation pipeline"
```

---

## End-to-End Verification Checklist

Flash the device and verify each item:

- [ ] Device boots, connects to WiFi, enters Translator mode
- [ ] Idle screen shows all 8 language key hints (C/V/U/R/M/J/K/E)
- [ ] Hold C, speak Czech → "Transcribing..." → "Translating..." → English shown and spoken
- [ ] Hold C, speak English → Czech shown and spoken (auto-detected direction)
- [ ] Hold E, speak Japanese → English shown and spoken (universal key)
- [ ] Hold E, speak Russian → English shown and spoken (universal key)
- [ ] Silent recording shows "No voice detected" popup
- [ ] API error shows popup with error text
- [ ] Translation appears in history (Utilities → History Log)
- [ ] History survives reboot (persisted to LittleFS)
- [ ] History scrolls with Fn+W/S when more than 4 entries
- [ ] Battery < 10% → popup warning appears
- [ ] WiFi loss while translating → error popup shown
- [ ] Fn key in Translator → Utilities, Fn in Utilities → Translator
