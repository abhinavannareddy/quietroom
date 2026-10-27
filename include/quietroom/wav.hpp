#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "quietroom/types.hpp"

namespace quietroom {

struct WavData {
    std::uint32_t sample_rate = 0;
    std::vector<float> samples;  // mono, -1..1
};

// Reads 16-bit PCM or 32-bit float mono WAV at 16 kHz. Returns false and fills `error` on
// anything else (stereo, other rates, truncated or malformed files). Never reads past the file.
bool read_wav(const std::string& path, WavData& out, std::string& error);

// Writes 16-bit PCM mono. Samples outside -1..1 are clipped.
bool write_wav(const std::string& path, const std::vector<float>& samples, std::uint32_t sample_rate,
               std::string& error);

}  // namespace quietroom
