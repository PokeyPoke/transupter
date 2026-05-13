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
    // Translate sourceText to targetLanguage (e.g. "English", "Czech")
    TranslationResult translate(const String& apiKey,
                                const String& sourceText,
                                const String& targetLanguage);
private:
    HttpClient _http;
};
