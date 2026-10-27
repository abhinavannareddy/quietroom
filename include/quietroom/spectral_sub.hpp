#pragma once

#include <array>

#include "quietroom/config.hpp"
#include "quietroom/types.hpp"

namespace quietroom {

// Power spectral subtraction with a spectral floor and one-pole gain smoothing over time.
//   S = max(P - alpha * N, beta * P)      G_raw = sqrt(S / P)
//   G[t] = s * G[t-1] + (1 - s) * G_raw
class SpectralSubtractor {
   public:
    explicit SpectralSubtractor(const Config& config);

    // power, noise: kBins values. gain_out: kBins smoothed amplitude gains in [sqrt(beta), 1].
    void apply(const Sample* power, const Sample* noise, Sample* gain_out);
    void reset();

   private:
    Config config_;
    std::array<Sample, kBins> previous_gain_;
};

}  // namespace quietroom
