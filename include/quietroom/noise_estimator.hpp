#pragma once

#include <array>

#include "quietroom/config.hpp"
#include "quietroom/types.hpp"

namespace quietroom {

// Noise power spectrum estimate (257 bins). The first noise_init_frames frames are averaged
// unconditionally (assumed noise-only); afterwards it is updated with exponential smoothing,
// and only on frames the VAD calls non-speech.
class NoiseEstimator {
   public:
    explicit NoiseEstimator(const Config& config);

    void update(const Sample* power, bool is_speech);
    void reset();

    const Sample* estimate() const { return noise_.data(); }
    bool initialised() const { return frames_ >= config_.noise_init_frames; }

   private:
    Config config_;
    std::array<Sample, kBins> noise_;
    std::size_t frames_ = 0;
};

}  // namespace quietroom
