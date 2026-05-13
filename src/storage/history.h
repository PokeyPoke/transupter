#pragma once
#include <Arduino.h>
#include <vector>

struct HistoryEntry {
    String source;
    String translated;
    String langKey;
};

class History {
public:
    static constexpr int  MAX_ENTRIES = 50;
    static constexpr char PATH[]      = "/history.json";

    void begin();  // load existing entries from LittleFS
    void append(const String& source, const String& translated, const String& langKey);
    const std::vector<HistoryEntry>& entries() const;

private:
    std::vector<HistoryEntry> _entries;
    void save() const;
    void load();
};
