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
        langHint
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
    String lang = doc["language"].as<String>();
    if (text.isEmpty()) {
        return { "", lang, false, "Empty transcript" };
    }

    return { text, lang, true, "" };
}
