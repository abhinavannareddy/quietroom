#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <random>
#include <vector>

#include "quietroom/fft.hpp"
#include "quietroom/vad.hpp"

using namespace quietroom;

namespace {

std::array<Sample, kBins> power_of(const Fft& fft, const Sample* hop) {
    std::array<Complex, kFftSize> buf{};
    for (std::size_t i = 0; i < kHop; ++i) buf[i] = {hop[i], 0};
    fft.forward(buf.data());
    std::array<Sample, kBins> p{};
    for (std::size_t k = 0; k < kBins; ++k) p[k] = buf[k].re * buf[k].re + buf[k].im * buf[k].im;
    return p;
}

struct Source {
    std::mt19937 rng{3};
    std::uniform_real_distribution<float> u{-1.0f, 1.0f};
    std::size_t t = 0;

    // Quiet broadband noise.
    std::array<Sample, kHop> noise(float amp = 0.01f) {
        std::array<Sample, kHop> f{};
        for (auto& v : f) v = static_cast<Sample>(amp * u(rng));
        t += kHop;
        return f;
    }
    // Voiced-like: 150 Hz harmonics with decaying amplitude, plus a little noise.
    std::array<Sample, kHop> voiced() {
        std::array<Sample, kHop> f{};
        const double pi = std::acos(-1.0);
        for (std::size_t i = 0; i < kHop; ++i) {
            double s = 0;
            for (int h = 1; h <= 10; ++h) {
                s += std::sin(2.0 * pi * 150.0 * h * static_cast<double>(t + i) / 16000.0) / h;
            }
            f[i] = static_cast<Sample>(0.2 * s + 0.005 * u(rng));
        }
        t += kHop;
        return f;
    }
};

}  // namespace

TEST(VadFeatures, EnergyDbOrdersByLevel) {
    std::array<Sample, kHop> quiet{}, loud{};
    quiet.fill(0.001f);
    loud.fill(0.5f);
    EXPECT_LT(frame_energy_db(quiet.data()), frame_energy_db(loud.data()) - 40.0f);
    EXPECT_NEAR(frame_energy_db(loud.data()), 10.0f * std::log10(0.25f), 0.01f);
}

TEST(VadFeatures, ZeroCrossingRateOfKnownSignals) {
    std::array<Sample, kHop> alt{}, dc{}, tone{};
    for (std::size_t i = 0; i < kHop; ++i) {
        alt[i] = (i % 2) ? 1.0f : -1.0f;
        dc[i] = 1.0f;
        tone[i] = static_cast<Sample>(
            std::sin(2.0 * std::acos(-1.0) * 1000.0 * static_cast<double>(i) / 16000.0 + 0.3));
    }
    EXPECT_NEAR(zero_crossing_rate(alt.data()), 1.0f, 1e-6f);
    EXPECT_NEAR(zero_crossing_rate(dc.data()), 0.0f, 1e-6f);
    EXPECT_NEAR(zero_crossing_rate(tone.data()), 0.125f, 0.01f);
}

TEST(VadFeatures, FlatnessSeparatesNoiseFromTone) {
    Fft fft;
    Source src;
    const auto noise = src.noise(0.3f);
    const auto voiced = src.voiced();
    const auto pn = power_of(fft, noise.data());
    const auto pv = power_of(fft, voiced.data());
    EXPECT_GT(spectral_flatness(pn.data()), 0.3f);
    EXPECT_LT(spectral_flatness(pv.data()), 0.2f);
}

TEST(Vad, DetectsBurstAndHangoverEndsOnTime) {
    Config cfg;
    Vad vad(cfg);
    Fft fft;
    Source src;
    auto step = [&](const std::array<Sample, kHop>& f) {
        const auto p = power_of(fft, f.data());
        return vad.update(f.data(), p.data());
    };
    for (int i = 0; i < 60; ++i) EXPECT_FALSE(step(src.noise())) << "noise frame " << i;
    int detected = 0;
    for (int i = 0; i < 40; ++i) detected += step(src.voiced()) ? 1 : 0;
    EXPECT_GE(detected, 39);  // allow one frame for the first onset

    int tail_speech = 0;
    for (int i = 0; i < 30; ++i) tail_speech += step(src.noise()) ? 1 : 0;
    EXPECT_EQ(tail_speech, static_cast<int>(cfg.hangover_frames));
}

TEST(Vad, FloorAdaptsToSlowlyRisingNoise) {
    Config cfg;
    Vad vad(cfg);
    Fft fft;
    Source src;
    for (int i = 0; i < 40; ++i) {
        const auto f = src.noise();
        const auto p = power_of(fft, f.data());
        vad.update(f.data(), p.data());
    }
    const Sample before = vad.floor_db();
    // Noise jumps by 20 dB and stays there; the floor must follow upward.
    for (int i = 0; i < 2000; ++i) {
        const auto f = src.noise(0.1f);
        const auto p = power_of(fft, f.data());
        vad.update(f.data(), p.data());
    }
    EXPECT_GT(vad.floor_db(), before + Sample{10});
}

TEST(Vad, RecoversWhenStreamStartsFromDigitalSilence) {
    // A gated or muted mic gives exact zeros first, which drags the energy floor to about
    // -100 dB. Ordinary background noise afterwards must not be called speech for long.
    Config cfg;
    Vad vad(cfg);
    Fft fft;
    Source src;
    std::array<Sample, kHop> zeros{};
    auto step = [&](const std::array<Sample, kHop>& f) {
        const auto p = power_of(fft, f.data());
        return vad.update(f.data(), p.data());
    };
    for (int i = 0; i < 50; ++i) step(zeros);
    int speech_in_last_100 = 0;
    for (int i = 0; i < 700; ++i) {
        const bool s = step(src.noise());
        if (i >= 600) speech_in_last_100 += s ? 1 : 0;
    }
    EXPECT_EQ(speech_in_last_100, 0);
}
