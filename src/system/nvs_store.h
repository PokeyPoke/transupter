#pragma once
#include <Arduino.h>

class NvsStore {
public:
    void        begin();
    void        end();

    String      getString(const char* key, const char* defaultVal = "") const;
    void        putString(const char* key, const String& value);
    bool        hasKey(const char* key) const;
    void        remove(const char* key);
};
