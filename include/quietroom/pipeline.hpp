#pragma once

#include <array>

#include "quietroom/config.hpp"
#include "quietroom/fft.hpp"
#include "quietroom/framing.hpp"
#include "quietroom/noise_estimator.hpp"
#include "quietroom/spectral_sub.hpp"
#include "quietroom/types.hpp"
#include "quietroom/vad.hpp"

namespace quietroom {

// Streaming noise remover. Feed it 160-sample hops; it returns 160-sample hops that are the
// input delayed by exactly one hop (10 ms). Counted from when a sample arrives until it can be
// played, the latency is 10 to 20 ms, so the worst case is the 20 ms analysis window.
// Everything is preallocated in the constructor. NaN/Inf input samples are treated as 0 and
// out-of-range Config values are clamped (see Config::clamped()).
class Pipeline {
   public:
    explicit Pipeline(const Config& config = Config{});

    // in and out each point at kHop samples. Does not allocate, lock or throw.
    void process_frame(const Sample* in, Sample* out) noexcept;

    bool last_frame_was_speech() const { return last_was_speech_; }
    std::size_t frames_processed() const { return frames_; }
    const Config& config() const { return config_; }
    void reset();

   private:
    Config config_;
    Fft fft_;
    OverlapAdd ola_;
    Vad vad_;
    NoiseEstimator noise_;
    SpectralSubtractor subtractor_;

    std::array<Sample, kHop> hop_;  // sanitised copy of the input hop
    std::array<Sample, kWindow> frame_;
    std::array<Complex, kFftSize> spectrum_;
    std::array<Sample, kBins> power_;
    std::array<Sample, kBins> gain_;
    std::size_t frames_ = 0;
    bool last_was_speech_ = false;
};

}  // namespace quietroom
