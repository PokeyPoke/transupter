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

    // Try PSRAM first; fall back to 64KB DRAM buffer if unavailable
    size_t   bufSize = MAX_TTS_BYTES;
    uint8_t* buf     = (uint8_t*) ps_malloc(MAX_TTS_BYTES);
    if (!buf) {
        bufSize = 65536; // 64KB from DRAM — ~1.5s of TTS audio
        buf     = (uint8_t*) malloc(bufSize);
    }
    if (!buf) return { nullptr, 0, false, "Alloc failed" };

    size_t outLen = 0;
    auto resp = _http.postJsonBinary(
        OPENAI_HOST, OPENAI_TTS_PATH,
        apiKey, body,
        buf, bufSize, outLen
    );

    if (!resp.ok() || outLen == 0) {
        Serial.printf("[TTS] HTTP %d outLen=%u\n", resp.statusCode, (unsigned)outLen);
        free(buf);
        // Include first 20 chars of body so the on-screen error is meaningful
        String errBody = (resp.body.length() > 0) ? resp.body.substring(0, 20) : "no body";
        return { nullptr, 0, false, String(resp.statusCode) + " " + errBody };
    }

    return { buf, outLen, true, "" };
}
