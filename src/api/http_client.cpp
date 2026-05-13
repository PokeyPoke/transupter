#include "http_client.h"
#include "../config.h"
#include <HTTPClient.h>
#include <lwip/dns.h>
#include <lwip/ip_addr.h>

// Called before every API connection — overrides router DNS at lwIP level.
// Router was hijacking api.groq.com etc. to a local 10.x.x.x IP.
// Must be called AFTER lwIP is fully initialised (i.e. after WiFi connects),
// not during WiFi setup.
static void ensureGoogleDNS() {
    ip_addr_t dns;
    IP4_ADDR(&dns.u_addr.ip4, 8, 8, 8, 8);
    dns.type = IPADDR_TYPE_V4;
    dns_setserver(0, &dns);
}

static const char* MULTIPART_BOUNDARY = "----ESP32Boundary7MA4YWxk";

// Read response body in 128-byte blocks — reduces heap fragmentation vs byte-by-byte.
static String readBody(WiFiClientSecure& client) {
    String body;
    body.reserve(512);
    uint8_t tmp[128];
    unsigned long lastData = millis();
    while (millis() - lastData < 10000) {
        int avail = client.available();
        if (avail > 0) {
            int n = client.readBytes(tmp, min(avail, (int)sizeof(tmp)));
            if (n > 0) {
                body.concat((const char*)tmp, n);
                lastData = millis();
            }
        }
        if (!client.connected() && !client.available()) break;
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
        int avail = client.available();
        if (avail > 0) {
            int n = client.readBytes(buf + len,
                                     min(avail, (int)(maxLen - len)));
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
    ensureGoogleDNS();
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
    ensureGoogleDNS();
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
    ensureGoogleDNS();
    WiFiClientSecure client;
    client.setInsecure();

    if (!client.connect(host, 443, 15000)) {
        return { -1, "TLS connect failed" };
    }

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
    client.flush(); // ensure all bytes are sent before reading response

    // Read status line and headers with inactivity timeout
    client.setTimeout(15); // 15s per readStringUntil call
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
    ensureGoogleDNS();
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
