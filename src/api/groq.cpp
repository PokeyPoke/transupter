#include "groq.h"
#include "../config.h"
#include <ArduinoJson.h>

TranscriptResult GroqApi::transcribe(const String& apiKey,
                                     const uint8_t* wavData, size_t wavLen,
                                     const char* langHint) {
    HttpResponse resp = { 0, "" };
    for (int attempt = 0; attempt < 3; attempt++) {
        resp = _http.postMultipart(
            GROQ_HOST, GROQ_STT_PATH,
            apiKey, wavData, wavLen,
            GROQ_STT_MODEL, langHint
        );
        if (resp.ok()) break;
        Serial.printf("[Groq] attempt %d failed code=%d body='%s'\n",
                      attempt + 1, resp.statusCode, resp.body.substring(0,40).c_str());
        if (attempt < 2) delay(1500);
    }

    if (!resp.ok()) {
        return { "", "", false, "Groq " + String(resp.statusCode) + ":" + resp.body.substring(0,20) };
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
