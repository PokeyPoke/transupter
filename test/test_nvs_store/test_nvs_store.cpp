#include <unity.h>
#include <string>
#include <unordered_map>

static std::unordered_map<std::string, std::string> fakeNvs;

void setUp(void) {
    fakeNvs.clear();
}

void tearDown(void) {
}

std::string nvsGet(const std::string& key, const std::string& def) {
    auto it = fakeNvs.find(key);
    return it != fakeNvs.end() ? it->second : def;
}

void nvsPut(const std::string& key, const std::string& val) {
    fakeNvs[key] = val;
}

void test_put_and_get_returns_stored_value() {
    nvsPut("ssid", "MyNetwork");
    TEST_ASSERT_EQUAL_STRING("MyNetwork", nvsGet("ssid", "").c_str());
}

void test_get_missing_key_returns_default() {
    TEST_ASSERT_EQUAL_STRING("default", nvsGet("missing_key", "default").c_str());
}

void test_overwrite_existing_key() {
    nvsPut("ssid", "First");
    nvsPut("ssid", "Second");
    TEST_ASSERT_EQUAL_STRING("Second", nvsGet("ssid", "").c_str());
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_put_and_get_returns_stored_value);
    RUN_TEST(test_get_missing_key_returns_default);
    RUN_TEST(test_overwrite_existing_key);
    return UNITY_END();
}
