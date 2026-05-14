#include "http_client.h"
#include "../config.h"
#include <HTTPClient.h>
#include <WiFi.h>

static const char* MULTIPART_BOUNDARY = "----ESP32Boundary7MA4YWxk";

// WiFiClientSecure::setTimeout() takes SECONDS on ESP32 Arduino.
static constexpr int HTTP_READ_TIMEOUT_S = 15;

// Drain HTTP response headers, return true if Transfer-Encoding: chunked.
static bool drainHeaders(WiFiClientSecure& client) {
    bool chunked = false;
    int lines = 0;
    while (client.connected() && lines < 100) {
        String line = client.readStringUntil('\n');
        lines++;
        if (line.startsWith("Transfer-Encoding:") && line.indexOf("chunked") >= 0)
            chunked = true;
        if (line == "\r" || line == "\r\n" || line.isEmpty()) break;
    }
    return chunked;
}

// Read a plain (non-chunked) response body into a String.
static String readBodyPlain(WiFiClientSecure& client) {
    String body;
    body.reserve(4096);
    uint8_t tmp[256];
    unsigned long lastData = millis();
    while (millis() - lastData < 10000) {
        int avail = client.available();
        if (avail > 0) {
            int n = client.readBytes(tmp, min(avail, (int)sizeof(tmp)));
            if (n > 0) { body.concat((const char*)tmp, n); lastData = millis(); }
        }
        if (!client.connected() && !client.available()) break;
        delay(1);
    }
    return body;
}

// Decode chunked transfer encoding into a String.
static String readBodyChunked(WiFiClientSecure& client) {
    String body;
    body.reserve(4096);
    unsigned long lastData = millis();
    while ((client.connected() || client.available()) && millis() - lastData < 10000) {
        String sizeLine = client.readStringUntil('\n');
        sizeLine.trim();
        if (sizeLine.isEmpty()) { delay(1); continue; }

        char* endPtr = nullptr;
        long chunkSize = strtol(sizeLine.c_str(), &endPtr, 16);
        if (endPtr == sizeLine.c_str() || chunkSize < 0 || chunkSize > 1000000) break;
        if (chunkSize == 0) break;

        uint8_t tmp[256];
        long remaining = chunkSize;
        while (remaining > 0 && (client.connected() || client.available())) {
            int toRead = min((long)sizeof(tmp), remaining);
            int n = client.readBytes(tmp, toRead);
            if (n > 0) { body.concat((const char*)tmp, n); remaining -= n; lastData = millis(); }
            else delay(1);
        }
        client.readStringUntil('\n'); // consume trailing CRLF
    }
    return body;
}

// Decode chunked transfer encoding into a binary buffer.
static size_t readBodyBinaryChunked(WiFiClientSecure& client,
                                     uint8_t* buf, size_t maxLen) {
    size_t len = 0;
    unsigned long lastData = millis();
    while ((client.connected() || client.available()) && millis() - lastData < 10000) {
        String sizeLine = client.readStringUntil('\n');
        sizeLine.trim();
        if (sizeLine.isEmpty()) { delay(1); continue; }

        char* endPtr = nullptr;
        long chunkSize = strtol(sizeLine.c_str(), &endPtr, 16);
        if (endPtr == sizeLine.c_str() || chunkSize < 0 || chunkSize > 1000000) break;
        if (chunkSize == 0) break;

        size_t remaining = (size_t)chunkSize;
        while (remaining > 0 && len < maxLen && (client.connected() || client.available())) {
            size_t toRead = min(remaining, maxLen - len);
            int n = client.readBytes(buf + len, toRead);
            if (n > 0) { len += n; remaining -= n; lastData = millis(); }
            else delay(1);
        }
        client.readStringUntil('\n');
    }
    return len;
}

static String readBody(WiFiClientSecure& client, bool chunked) {
    return chunked ? readBodyChunked(client) : readBodyPlain(client);
}

static size_t readBodyBinary(WiFiClientSecure& client,
                              uint8_t* buf, size_t maxLen, bool chunked) {
    if (chunked) return readBodyBinaryChunked(client, buf, maxLen);
    size_t len = 0;
    unsigned long lastData = millis();
    while (millis() - lastData < 10000 && len < maxLen) {
        int avail = client.available();
        if (avail > 0) {
            int n = client.readBytes(buf + len, min(avail, (int)(maxLen - len)));
            if (n > 0) { len += n; lastData = millis(); }
        }
        if (!client.connected() && !client.available()) break;
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
    client.setTimeout(HTTP_READ_TIMEOUT_S);

    HTTPClient http;
    http.begin(client, String("https://") + host + path);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Authorization", "Bearer " + bearerToken);
    if (extraHeader) http.addHeader(extraHeader, extraHeaderVal);

    int code = http.POST(jsonBody);
    String body = http.getString();
    http.end();
    client.stop();
    return { code, body };
}

HttpResponse HttpClient::postJsonAnthropic(const char* host, const char* path,
                                           const String& apiKey,
                                           const String& jsonBody) {
    WiFiClientSecure client;
    client.setInsecure();
    client.setTimeout(HTTP_READ_TIMEOUT_S);

    HTTPClient http;
    http.begin(client, String("https://") + host + path);
    http.addHeader("Content-Type",      "application/json");
    http.addHeader("x-api-key",         apiKey);
    http.addHeader("anthropic-version", ANTHROPIC_VER);

    int code = http.POST(jsonBody);
    String body = http.getString();
    http.end();
    client.stop();
    return { code, body };
}

HttpResponse HttpClient::postMultipart(const char* host, const char* path,
                                       const String& bearerToken,
                                       const uint8_t* audioData, size_t audioLen,
                                       const char* model, const char* language) {
    Serial.printf("[Groq] heap=%u maxAlloc=%u audio=%u\n",
                  ESP.getFreeHeap(), ESP.getMaxAllocHeap(), (unsigned)audioLen);

    // Send a DNS probe using the carrier's own DNS before opening the TLS socket.
    // This exercises the WiFi transmit path and refreshes the iPhone hotspot's NAT
    // table entry for this device, preventing TCP RST on the first data packet.
    // Uses the carrier's DNS (not a hardcoded 8.8.8.8) so it works on any network.
    {
        IPAddress probe;
        bool ok = WiFi.hostByName(host, probe);
        Serial.printf("[Groq] DNS probe %s -> %s\n",
                      host, ok ? probe.toString().c_str() : "failed");
        // Failure is non-fatal — we still attempt the TLS connect below.
    }

    WiFiClientSecure client;
    client.setInsecure();

    Serial.printf("[Groq] connecting %s:443 ...\n", host);
    if (!client.connect(host, 443, 15000)) {
        char sslErr[64] = {};
        client.lastError(sslErr, sizeof(sslErr));
        Serial.printf("[Groq] FAILED ssl=%s\n", sslErr);
        client.stop();
        return { -1, String("TLS:") + sslErr };
    }
    // Set read timeout AFTER connect so SO_RCVTIMEO is applied to the live socket.
    client.setTimeout(HTTP_READ_TIMEOUT_S);
    // Brief settle: let the network stack drain any post-handshake frames
    // before we start writing. Prevents CONN_RESET on the first send.
    delay(50);
    Serial.printf("[Groq] connected, heap=%u\n", ESP.getFreeHeap());

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

    if (!client.connected()) {
        client.stop();
        return { -1, "Dropped after TLS" };
    }

    int sent = client.printf("POST %s HTTP/1.1\r\n", path);
    sent    += client.printf("Host: %s\r\n", host);
    sent    += client.printf("Authorization: Bearer %s\r\n", bearerToken.c_str());
    sent    += client.printf("Content-Type: multipart/form-data; boundary=%s\r\n", MULTIPART_BOUNDARY);
    sent    += client.printf("Content-Length: %u\r\n", (unsigned)contentLen);
    sent    += client.print("Connection: close\r\n\r\n");
    if (sent == 0) {
        client.stop();
        return { -1, "Send hdr failed" };
    }

    client.print(part1);

    static constexpr size_t WRITE_CHUNK = 1024;
    size_t written = 0;
    while (written < audioLen) {
        size_t n = min(WRITE_CHUNK, audioLen - written);
        size_t w = client.write(audioData + written, n);
        if (w == 0) {
            Serial.printf("[Groq] write stalled at %u/%u\n", (unsigned)written, (unsigned)audioLen);
            break;
        }
        written += w;
        yield();
    }
    Serial.printf("[Groq] sent %u/%u audio bytes\n", (unsigned)written, (unsigned)audioLen);

    client.print(part2);
    client.print(part3);
    client.print(ending);
    client.flush();

    String statusLine = client.readStringUntil('\n');
    int code = 0;
    sscanf(statusLine.c_str(), "HTTP/1.1 %d", &code);
    Serial.printf("[Groq] status=%d\n", code);
    bool chunked = drainHeaders(client);

    String body = readBody(client, chunked);
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
    client.setTimeout(HTTP_READ_TIMEOUT_S);

    if (!client.connect(host, 443, 15000)) {
        client.stop();
        return { -1, "TLS connect failed" };
    }

    client.printf("POST %s HTTP/1.1\r\n", path);
    client.printf("Host: %s\r\n", host);
    client.printf("Authorization: Bearer %s\r\n", bearerToken.c_str());
    client.print("Content-Type: application/json\r\n");
    client.printf("Content-Length: %u\r\n", (unsigned)jsonBody.length());
    client.print("Connection: close\r\n\r\n");
    client.print(jsonBody);
    client.flush();

    String statusLine = client.readStringUntil('\n');
    int code = 0;
    sscanf(statusLine.c_str(), "HTTP/1.1 %d", &code);
    bool chunked = drainHeaders(client);

    outLen = readBodyBinary(client, outBuf, outBufSize, chunked);
    client.stop();
    return { code, "" };
}
