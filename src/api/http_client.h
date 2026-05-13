#pragma once
#include <Arduino.h>
#include <WiFiClientSecure.h>

struct HttpResponse {
    int    statusCode;
    String body;
    bool   ok() const { return statusCode >= 200 && statusCode < 300; }
};

class HttpClient {
public:
    // JSON POST — standard Bearer auth
    HttpResponse postJson(const char* host, const char* path,
                          const String& bearerToken,
                          const String& jsonBody,
                          const char* extraHeader = nullptr,
                          const char* extraHeaderVal = nullptr);

    // Anthropic-specific: x-api-key + anthropic-version headers
    HttpResponse postJsonAnthropic(const char* host, const char* path,
                                   const String& apiKey,
                                   const String& jsonBody);

    // Multipart POST for binary audio — returns response body as String
    HttpResponse postMultipart(const char* host, const char* path,
                               const String& bearerToken,
                               const uint8_t* audioData, size_t audioLen,
                               const char* model,
                               const char* language);

    // Binary response POST — fills outBuf, sets outLen
    HttpResponse postJsonBinary(const char* host, const char* path,
                                const String& bearerToken,
                                const String& jsonBody,
                                uint8_t* outBuf, size_t outBufSize,
                                size_t& outLen);
};
