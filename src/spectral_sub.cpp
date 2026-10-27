#include "quietroom/spectral_sub.hpp"

#include <algorithm>
#include <cmath>

namespace quietroom {

namespace {
constexpr Sample kPowerEps = 1e-20f;
}

SpectralSubtractor::SpectralSubtractor(const Config& config) : config_(config), previous_gain_{} {
    previous_gain_.fill(1);
}

void SpectralSubtractor::reset() { previous_gain_.fill(1); }

void SpectralSubtractor::apply(const Sample* power, const Sample* noise, Sample* gain_out) {
    const Sample alpha = config_.over_subtraction;
    const Sample beta = config_.spectral_floor;
    const Sample s = config_.gain_smoothing;
    for (std::size_t k = 0; k < kBins; ++k) {
        const Sample p = power[k] + kPowerEps;
        const Sample subtracted = std::max(p - alpha * noise[k], beta * p);
        const Sample raw = std::sqrt(subtracted / p);
        const Sample g = s * previous_gain_[k] + (Sample{1} - s) * raw;
        previous_gain_[k] = g;
        gain_out[k] = g;
    }
}

}  // namespace quietroom
