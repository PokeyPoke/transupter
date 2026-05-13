#include "nvs_store.h"
#include "../config.h"
#include <Preferences.h>

static Preferences prefs;

void NvsStore::begin() {
    prefs.begin(NVS_NS, false);
}

void NvsStore::end() {
    prefs.end();
}

String NvsStore::getString(const char* key, const char* defaultVal) const {
    return prefs.getString(key, defaultVal);
}

void NvsStore::putString(const char* key, const String& value) {
    prefs.putString(key, value);
}

bool NvsStore::hasKey(const char* key) const {
    return prefs.isKey(key);
}

void NvsStore::remove(const char* key) {
    prefs.remove(key);
}
