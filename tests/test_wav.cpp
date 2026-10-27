#include <gtest/gtest.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <limits>
#include <string>
#include <vector>

#include "quietroom/wav.hpp"

using namespace quietroom;

namespace {
std::string temp_path(const char* name) { return std::string("quietroom_test_") + name; }

void write_bytes(const std::string& path, const std::vector<unsigned char>& bytes) {
    std::ofstream f(path, std::ios::binary);
    f.write(reinterpret_cast<const char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
}
}  // namespace

TEST(Wav, RoundTripsSixteenBitMono) {
    const std::string path = temp_path("roundtrip.wav");
    std::vector<float> samples = {0.0f, 0.5f, -0.5f, 0.999f, -1.0f, 0.25f};
    std::string error;
    ASSERT_TRUE(write_wav(path, samples, kSampleRate, error)) << error;

    WavData read;
    ASSERT_TRUE(read_wav(path, read, error)) << error;
    ASSERT_EQ(read.samples.size(), samples.size());
    EXPECT_EQ(read.sample_rate, kSampleRate);
    for (std::size_t i = 0; i < samples.size(); ++i)
        EXPECT_NEAR(read.samples[i], samples[i], 1.0f / 32768.0f);
    std::remove(path.c_str());
}

TEST(Wav, ClipsOutOfRangeSamplesInsteadOfWrapping) {
    const std::string path = temp_path("clip.wav");
    std::string error;
    ASSERT_TRUE(write_wav(path, {2.0f, -2.0f}, kSampleRate, error)) << error;
    WavData read;
    ASSERT_TRUE(read_wav(path, read, error)) << error;
    EXPECT_GT(read.samples[0], 0.99f);
    EXPECT_LT(read.samples[1], -0.99f);
    std::remove(path.c_str());
}

TEST(Wav, MissingFileReportsError) {
    WavData read;
    std::string error;
    EXPECT_FALSE(read_wav("definitely_not_here.wav", read, error));
    EXPECT_FALSE(error.empty());
}

TEST(Wav, RejectsGarbageAndTruncatedHeaders) {
    WavData read;
    std::string error;
    const std::string path = temp_path("garbage.wav");
    write_bytes(path, {'R', 'I', 'F', 'F', 1, 2});
    EXPECT_FALSE(read_wav(path, read, error));
    write_bytes(path, std::vector<unsigned char>(64, 0x41));
    EXPECT_FALSE(read_wav(path, read, error));
    std::remove(path.c_str());
}

TEST(Wav, RejectsWrongSampleRate) {
    const std::string path = temp_path("rate.wav");
    std::string error;
    ASSERT_TRUE(write_wav(path, {0.0f, 0.1f}, 44100, error)) << error;
    WavData read;
    EXPECT_FALSE(read_wav(path, read, error));
    EXPECT_NE(error.find("16000"), std::string::npos) << error;
    std::remove(path.c_str());
}

TEST(Wav, DataChunkLargerThanFileIsClampedNotOverread) {
    const std::string path = temp_path("lying.wav");
    std::string error;
    ASSERT_TRUE(write_wav(path, {0.1f, 0.2f, 0.3f, 0.4f}, kSampleRate, error)) << error;
    // Corrupt the data chunk size (bytes 40..43) to claim far more than the file holds.
    std::fstream f(path, std::ios::in | std::ios::out | std::ios::binary);
    f.seekp(40);
    const unsigned char big[4] = {0xFF, 0xFF, 0xFF, 0x7F};
    f.write(reinterpret_cast<const char*>(big), 4);
    f.close();
    WavData read;
    ASSERT_TRUE(read_wav(path, read, error)) << error;
    EXPECT_EQ(read.samples.size(), 4u);
    std::remove(path.c_str());
}

namespace {

void le(std::vector<unsigned char>& b, std::uint32_t v, int bytes) {
    for (int i = 0; i < bytes; ++i) b.push_back(static_cast<unsigned char>((v >> (8 * i)) & 0xFF));
}
void tag(std::vector<unsigned char>& b, const char* t) { b.insert(b.end(), t, t + 4); }

// Minimal mono 16 kHz WAV with a configurable fmt chunk and raw data bytes.
std::vector<unsigned char> make_wav(std::uint16_t format_tag, std::uint16_t bits,
                                    const std::vector<unsigned char>& fmt_extension,
                                    const std::vector<unsigned char>& data) {
    std::vector<unsigned char> b;
    tag(b, "RIFF");
    le(b, static_cast<std::uint32_t>(36 + fmt_extension.size() + data.size()), 4);
    tag(b, "WAVE");
    tag(b, "fmt ");
    le(b, static_cast<std::uint32_t>(16 + fmt_extension.size()), 4);
    le(b, format_tag, 2);
    le(b, 1, 2);
    le(b, 16000, 4);
    le(b, 16000u * bits / 8, 4);
    le(b, bits / 8, 2);
    le(b, bits, 2);
    b.insert(b.end(), fmt_extension.begin(), fmt_extension.end());
    tag(b, "data");
    le(b, static_cast<std::uint32_t>(data.size()), 4);
    b.insert(b.end(), data.begin(), data.end());
    return b;
}

}  // namespace

TEST(WavHostile, NonFiniteFloatSamplesAreZeroedAndLargeOnesClamped) {
    std::vector<unsigned char> data;
    for (float v : {std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(),
                    0.5f, 1e30f}) {
        std::uint32_t raw;
        std::memcpy(&raw, &v, 4);
        le(data, raw, 4);
    }
    const std::string path = temp_path("nonfinite.wav");
    write_bytes(path, make_wav(3, 32, {}, data));
    WavData read;
    std::string error;
    ASSERT_TRUE(read_wav(path, read, error)) << error;
    ASSERT_EQ(read.samples.size(), 4u);
    EXPECT_EQ(read.samples[0], 0.0f);
    EXPECT_EQ(read.samples[1], 0.0f);
    EXPECT_EQ(read.samples[2], 0.5f);
    EXPECT_EQ(read.samples[3], 1.0f);
    std::remove(path.c_str());
}

TEST(WavHostile, ChunkSizeOfFourGigabytesBeforeDataFailsCleanly) {
    std::vector<unsigned char> b;
    tag(b, "RIFF");
    le(b, 100, 4);
    tag(b, "WAVE");
    tag(b, "junk");
    le(b, 0xFFFFFFFFu, 4);
    b.insert(b.end(), 32, 0);
    const std::string path = temp_path("hugechunk.wav");
    write_bytes(path, b);
    WavData read;
    std::string error;
    EXPECT_FALSE(read_wav(path, read, error));
    EXPECT_FALSE(error.empty());
    std::remove(path.c_str());
}

TEST(WavHostile, ExtensibleFormatWithIntegerSubformatAndThirtyTwoBitsIsRejected) {
    // WAVE_FORMAT_EXTENSIBLE: cbSize(2) validBits(2) channelMask(4) subFormat GUID(16).
    std::vector<unsigned char> ext;
    le(ext, 22, 2);
    le(ext, 32, 2);
    le(ext, 0, 4);
    le(ext, 1, 2);  // sub-format 1 = integer PCM, but 32-bit: we only accept 16-bit integer PCM
    ext.insert(ext.end(), 14, 0);
    const std::string path = temp_path("ext32.wav");
    write_bytes(path, make_wav(0xFFFE, 32, ext, std::vector<unsigned char>(16, 0x11)));
    WavData read;
    std::string error;
    EXPECT_FALSE(read_wav(path, read, error));
    std::remove(path.c_str());
}

TEST(WavHostile, ZeroLengthDataChunkGivesEmptySignalNotACrash) {
    const std::string path = temp_path("emptydata.wav");
    write_bytes(path, make_wav(1, 16, {}, {}));
    WavData read;
    std::string error;
    ASSERT_TRUE(read_wav(path, read, error)) << error;
    EXPECT_TRUE(read.samples.empty());
    std::remove(path.c_str());
}
