#include "quietroom/noise_estimator.hpp"

namespace quietroom {

NoiseEstimator::NoiseEstimator(const Config& config) : config_(config), noise_{} {}

void NoiseEstimator::reset() {
    noise_.fill(0);
    frames_ = 0;
}

void NoiseEstimator::update(const Sample* power, bool is_speech) {
    if (!initialised()) {
        // Running mean over the start-up frames.
        const Sample inv = Sample{1} / static_cast<Sample>(frames_ + 1);
        for (std::size_t k = 0; k < kBins; ++k) noise_[k] += (power[k] - noise_[k]) * inv;
        ++frames_;
        return;
    }
    if (is_speech) return;
    const Sample a = config_.noise_smoothing;
    for (std::size_t k = 0; k < kBins; ++k) noise_[k] = a * noise_[k] + (Sample{1} - a) * power[k];
}

}  // namespace quietroom
