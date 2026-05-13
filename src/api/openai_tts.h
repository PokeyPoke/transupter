#pragma once
#include <Arduino.h>
#include <cstdint>
#include <cstddef>
#include "http_client.h"

struct TtsResult {
    uint8_t* data;    // WAV bytes in PSRAM (caller must free())
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
