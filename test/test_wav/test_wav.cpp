#include <unity.h>
#include <cstdint>
#include <cstring>

// Inline WAV builder duplicated here so native test is self-contained
#pragma pack(push, 1)
struct WavHeader {
    char     riff[4]       = {'R','I','F','F'};
    uint32_t fileSize;
    char     wave[4]       = {'W','A','V','E'};
    char     fmt[4]        = {'f','m','t',' '};
    uint32_t fmtSize       = 16;
    uint16_t audioFmt      = 1;
    uint16_t channels      = 1;
    uint32_t sampleRate;
    uint32_t byteRate;
    uint16_t blockAlign;
    uint16_t bitsPerSample = 16;
    char     data[4]       = {'d','a','t','a'};
    uint32_t dataSize;
};
#pragma pack(pop)

WavHeader buildWavHeader(uint32_t sampleRate, uint32_t numSamples) {
    WavHeader h;
    h.sampleRate  = sampleRate;
    h.dataSize    = numSamples * sizeof(int16_t);
    h.blockAlign  = 1 * (16 / 8);
    h.byteRate    = sampleRate * h.blockAlign;
    h.fileSize    = 36 + h.dataSize;
    return h;
}

void setUp() {}
void tearDown() {}

void test_wav_header_size_is_44_bytes() {
    TEST_ASSERT_EQUAL(44, (int)sizeof(WavHeader));
}

void test_wav_header_riff_tag() {
    WavHeader h = buildWavHeader(16000, 16000);
    TEST_ASSERT_EQUAL_CHAR_ARRAY("RIFF", h.riff, 4);
    TEST_ASSERT_EQUAL_CHAR_ARRAY("WAVE", h.wave, 4);
    TEST_ASSERT_EQUAL_CHAR_ARRAY("fmt ", h.fmt,  4);
    TEST_ASSERT_EQUAL_CHAR_ARRAY("data", h.data, 4);
}

void test_wav_header_file_size_for_1_second_16khz() {
    WavHeader h = buildWavHeader(16000, 16000);
    TEST_ASSERT_EQUAL_UINT32(32036, h.fileSize);
    TEST_ASSERT_EQUAL_UINT32(32000, h.dataSize);
}

void test_wav_header_byte_rate() {
    WavHeader h = buildWavHeader(16000, 1000);
    TEST_ASSERT_EQUAL_UINT32(32000, h.byteRate);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_wav_header_size_is_44_bytes);
    RUN_TEST(test_wav_header_riff_tag);
    RUN_TEST(test_wav_header_file_size_for_1_second_16khz);
    RUN_TEST(test_wav_header_byte_rate);
    return UNITY_END();
}
