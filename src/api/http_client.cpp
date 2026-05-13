#include "http_client.h"
#include "../config.h"
#include <HTTPClient.h>

static const char* MULTIPART_BOUNDARY = "----ESP32Boundary7MA4YWxk";

// Read response body with a 10-second inactivity timeout.
// Handles servers that use chunked encoding or keep connections alive.
static String readBody(WiFiClientSecure& client) {
    String body;
    body.reserve(512);
    unsigned long lastData = millis();
    while (millis() - lastData < 10000) {
        while (client.available()) {
            body += (char)client.read();
            lastData = millis();
        }
        if (!client.connected()) break;
        delay(1);
    }
    return body;
}

// Same but reads into a binary buffer instead of a String.
static size_t readBodyBinary(WiFiClientSecure& client,
                              uint8_t* buf, size_t maxLen) {
    size_t len = 0;
    unsigned long lastData = millis();
    while (millis() - lastData < 10000 && len < maxLen) {
        while (client.available() && len < maxLen) {
            buf[len++] = client.read();
            lastData = millis();
        }
        if (!client.connected()) break;
        delay(1);
    }
    return len;
}

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

HttpResponse HttpClient::postJsonAnthropic(const char* host, const char* path,
                                           const String& apiKey,
                                           const String& jsonBody) {
    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    http.begin(client, String("https://") + host + path);
    http.addHeader("Content-Type",      "application/json");
    http.addHeader("x-api-key",         apiKey);
    http.addHeader("anthropic-version", ANTHROPIC_VER);

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

    // Explicit 15-second connection timeout (milliseconds for 3-arg connect)
    if (!client.connect(host, 443, 15000)) return { -1, "connect timeout" };

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

    client.printf("POST %s HTTP/1.1\r\n", path);
    client.printf("Host: %s\r\n", host);
    client.printf("Authorization: Bearer %s\r\n", bearerToken.c_str());
    client.printf("Content-Type: multipart/form-data; boundary=%s\r\n", MULTIPART_BOUNDARY);
    client.printf("Content-Length: %u\r\n", (unsigned)contentLen);
    client.print("Connection: close\r\n\r\n");

    client.print(part1);

    // Send audio in 1KB chunks — yields to WiFi stack between chunks
    static constexpr size_t WRITE_CHUNK = 1024;
    size_t written = 0;
    while (written < audioLen) {
        size_t n = min(WRITE_CHUNK, audioLen - written);
        if (client.write(audioData + written, n) == 0) break;
        written += n;
        yield();
    }

    client.print(part2);
    client.print(part3);
    client.print(ending);

    // Read status line and skip headers
    String statusLine = client.readStringUntil('\n');
    int code = 0;
    sscanf(statusLine.c_str(), "HTTP/1.1 %d", &code);
    while (client.connected()) {
        String line = client.readStringUntil('\n');
        if (line == "\r" || line == "\r\n" || line.isEmpty()) break;
    }

    String body = readBody(client);
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

    if (!client.connect(host, 443, 15000)) return { -1, "connect failed" };

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
        if (line == "\r" || line == "\r\n" || line.isEmpty()) break;
    }

    outLen = readBodyBinary(client, outBuf, outBufSize);
    client.stop();
    return { code, "" };
}
