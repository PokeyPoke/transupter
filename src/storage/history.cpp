#include "history.h"
#include <LittleFS.h>
#include <ArduinoJson.h>

constexpr char History::PATH[];

void History::begin() {
    load();
}

void History::append(const String& source, const String& translated, const String& langKey) {
    _entries.push_back({ source, translated, langKey });
    if ((int)_entries.size() > MAX_ENTRIES) {
        _entries.erase(_entries.begin());
    }
    save();
}

const std::vector<HistoryEntry>& History::entries() const { return _entries; }

void History::load() {
    if (!LittleFS.exists(PATH)) return;
    File f = LittleFS.open(PATH, "r");
    if (!f) return;

    JsonDocument doc;
    if (deserializeJson(doc, f)) { f.close(); return; }
    f.close();

    JsonArray arr = doc.as<JsonArray>();
    for (JsonObject obj : arr) {
        _entries.push_back({
            obj["src"].as<String>(),
            obj["tr"].as<String>(),
            obj["lang"].as<String>(),
        });
    }
}

void History::save() const {
    File f = LittleFS.open(PATH, "w");
    if (!f) return;

    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();
    for (auto& e : _entries) {
        JsonObject obj = arr.add<JsonObject>();
        obj["src"]  = e.source;
        obj["tr"]   = e.translated;
        obj["lang"] = e.langKey;
    }
    serializeJson(doc, f);
    f.close();
}
