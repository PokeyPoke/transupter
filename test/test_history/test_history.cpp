#include <unity.h>
#include <vector>
#include <string>

// ── In-memory mock of history logic ─────────────────────────────────
static constexpr int MAX_ENTRIES = 50;

struct HistoryEntry {
    std::string source;
    std::string translated;
    std::string langKey;
};

static std::vector<HistoryEntry> fakeHistory;

void historyAppend(const std::string& src, const std::string& tr, const std::string& lang) {
    fakeHistory.push_back({ src, tr, lang });
    if ((int)fakeHistory.size() > MAX_ENTRIES) {
        fakeHistory.erase(fakeHistory.begin());
    }
}

int historySize() { return (int)fakeHistory.size(); }
HistoryEntry historyGet(int i) { return fakeHistory[i]; }

void setUp()    { fakeHistory.clear(); }
void tearDown() {}

void test_append_single_entry() {
    historyAppend("Ahoj", "Hello", "c");
    TEST_ASSERT_EQUAL(1, historySize());
    TEST_ASSERT_EQUAL_STRING("Hello", historyGet(0).translated.c_str());
}

void test_append_respects_max_50() {
    for (int i = 0; i < 55; i++) {
        historyAppend("word", "translated", "c");
    }
    TEST_ASSERT_EQUAL(50, historySize());
}

void test_oldest_entry_evicted_when_full() {
    historyAppend("First", "Premier", "c");
    for (int i = 0; i < 50; i++) historyAppend("word", "mot", "c");
    bool found = false;
    for (int i = 0; i < historySize(); i++) {
        if (historyGet(i).source == "First") { found = true; break; }
    }
    TEST_ASSERT_FALSE(found);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_append_single_entry);
    RUN_TEST(test_append_respects_max_50);
    RUN_TEST(test_oldest_entry_evicted_when_full);
    return UNITY_END();
}
