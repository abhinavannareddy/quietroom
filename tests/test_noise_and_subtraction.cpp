#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <numeric>

#include "quietroom/noise_estimator.hpp"
#include "quietroom/spectral_sub.hpp"

using namespace quietroom;

namespace {
std::array<Sample, kBins> flat(Sample v) {
    std::array<Sample, kBins> a{};
    a.fill(v);
    return a;
}
Sample mean_of(const Sample* p) {
    return std::accumulate(p, p + kBins, Sample{0}) / static_cast<Sample>(kBins);
}
}  // namespace

TEST(NoiseEstimator, InitialisesFromFirstFramesRegardlessOfVad) {
    Config cfg;
    NoiseEstimator est(cfg);
    for (std::size_t i = 0; i < cfg.noise_init_frames; ++i) est.update(flat(4).data(), true);
    EXPECT_NEAR(mean_of(est.estimate()), 4.0f, 1e-4f);
}

TEST(NoiseEstimator, ConvergesOnStationaryNoise) {
    Config cfg;
    NoiseEstimator est(cfg);
    for (std::size_t i = 0; i < cfg.noise_init_frames; ++i) est.update(flat(1).data(), false);
    // Noise level steps to 3 and stays: exponential smoothing must converge to it.
    for (int i = 0; i < 300; ++i) est.update(flat(3).data(), false);
    EXPECT_NEAR(mean_of(est.estimate()), 3.0f, 0.01f);
}

TEST(NoiseEstimator, FreezesDuringSpeech) {
    Config cfg;
    NoiseEstimator est(cfg);
    for (std::size_t i = 0; i < cfg.noise_init_frames; ++i) est.update(flat(1).data(), false);
    const Sample before = mean_of(est.estimate());
    for (int i = 0; i < 100; ++i) est.update(flat(1000).data(), true);
    EXPECT_EQ(mean_of(est.estimate()), before);
}

TEST(SpectralSubtractor, KeepsStrongBinsAndFloorsNoiseOnlyBins) {
    Config cfg;
    cfg.gain_smoothing = 0;
    SpectralSubtractor sub(cfg);
    std::array<Sample, kBins> gain{};
    sub.apply(flat(1000).data(), flat(1).data(), gain.data());
    EXPECT_NEAR(gain[10], std::sqrt(1.0f - cfg.over_subtraction / 1000.0f), 1e-4f);
    sub.apply(flat(1).data(), flat(1).data(), gain.data());  // power == noise: fully subtracted
    EXPECT_NEAR(gain[10], std::sqrt(cfg.spectral_floor), 1e-4f);
}

TEST(SpectralSubtractor, GainSmoothingSlowsChanges) {
    Config fast, slow;
    fast.gain_smoothing = 0;
    slow.gain_smoothing = 0.9f;
    SpectralSubtractor a(fast), b(slow);
    std::array<Sample, kBins> ga{}, gb{};
    a.apply(flat(1).data(), flat(1).data(), ga.data());
    b.apply(flat(1).data(), flat(1).data(), gb.data());
    EXPECT_LT(ga[5], gb[5]);  // the smoothed gain is still near its initial 1.0
}

TEST(SpectralSubtractor, OverSubtractionChangesOutput) {
    Config lo, hi;
    lo.gain_smoothing = hi.gain_smoothing = 0;
    lo.over_subtraction = 1.0f;
    hi.over_subtraction = 3.0f;
    SpectralSubtractor a(lo), b(hi);
    std::array<Sample, kBins> ga{}, gb{};
    a.apply(flat(10).data(), flat(1).data(), ga.data());
    b.apply(flat(10).data(), flat(1).data(), gb.data());
    EXPECT_GT(ga[5], gb[5]);
}
