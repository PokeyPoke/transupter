#include "anthropic.h"
#include "../config.h"
#include <ArduinoJson.h>

TranslationResult AnthropicApi::translate(const String& apiKey,
                                          const String& sourceText,
                                          const String& targetLanguage) {
    String prompt = "Translate the following text to " + targetLanguage +
                    ". Output only the translated text with no explanation.\n\n" +
                    sourceText;

    JsonDocument req;
    req["model"]      = ANTHROPIC_MODEL;
    req["max_tokens"] = 1024;
    JsonArray msgs    = req["messages"].to<JsonArray>();
    JsonObject msg    = msgs.add<JsonObject>();
    msg["role"]       = "user";
    msg["content"]    = prompt;

    String body;
    serializeJson(req, body);

    auto resp = _http.postJsonAnthropic(
        ANTHROPIC_HOST, ANTHROPIC_PATH, apiKey, body
    );

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
