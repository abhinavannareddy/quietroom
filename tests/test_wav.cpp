#include <gtest/gtest.h>

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

#include "quietroom/wav.hpp"

using namespace quietroom;

namespace {
std::string temp_path(const char* name) { return std::string("quietroom_test_") + name; }

void write_bytes(const std::string& path, const std::vector<unsigned char>& bytes) {
    std::ofstream f(path, std::ios::binary);
    f.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
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
    for (std::size_t i = 0; i < samples.size(); ++i) EXPECT_NEAR(read.samples[i], samples[i], 1.0f / 32768.0f);
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
