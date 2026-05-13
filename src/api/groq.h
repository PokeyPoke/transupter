#pragma once
#include <Arduino.h>
#include "http_client.h"

struct TranscriptResult {
    String text;
    String detectedLang; // ISO 639-1 returned by Whisper
    bool   success;
    String error;
};

class GroqApi {
public:
    // langHint: ISO 639-1 code, or "" for auto-detect
    TranscriptResult transcribe(const String& apiKey,
                                const uint8_t* wavData, size_t wavLen,
                                const char* langHint = "");
private:
    HttpClient _http;
};
