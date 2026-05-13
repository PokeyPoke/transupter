#include "openai_tts.h"
#include "../config.h"
#include <ArduinoJson.h>
#include <esp32-hal-psram.h>

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
        free(buf);
        return { nullptr, 0, false, "TTS HTTP " + String(resp.statusCode) };
    }

    return { buf, outLen, true, "" };
}
