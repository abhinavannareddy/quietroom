#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <random>

#include "quietroom/fft.hpp"

using namespace quietroom;

TEST(Fft, RoundTripErrorUnderTolerance) {
    Fft fft;
    std::mt19937 rng(7);
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
    std::array<Complex, kFftSize> data{};
    std::array<Sample, kFftSize> original{};
    for (std::size_t i = 0; i < kFftSize; ++i) {
        original[i] = static_cast<Sample>(dist(rng));
        data[i] = {original[i], 0};
    }
    fft.forward(data.data());
    fft.inverse(data.data());
    Sample max_err = 0;
    for (std::size_t i = 0; i < kFftSize; ++i) {
        max_err = std::max(max_err, std::abs(data[i].re - original[i]));
        max_err = std::max(max_err, std::abs(data[i].im));
    }
    EXPECT_LT(max_err, 1e-5);
}

TEST(Fft, PureToneLandsInExpectedBin) {
    Fft fft;
    constexpr std::size_t kBin = 40;
    const double pi = std::acos(-1.0);
    std::array<Complex, kFftSize> data{};
    for (std::size_t i = 0; i < kFftSize; ++i) {
        data[i] = {static_cast<Sample>(std::cos(2.0 * pi * kBin * static_cast<double>(i) / kFftSize)), 0};
    }
    fft.forward(data.data());
    std::size_t peak = 0;
    Sample best = 0;
    for (std::size_t k = 0; k <= kFftSize / 2; ++k) {
        const Sample mag = data[k].re * data[k].re + data[k].im * data[k].im;
        if (mag > best) {
            best = mag;
            peak = k;
        }
    }
    EXPECT_EQ(peak, kBin);
    EXPECT_NEAR(std::sqrt(best), kFftSize / 2.0, 1e-3);
}

TEST(Fft, MatchesNaiveDftOnSmallInput) {
    Fft fft;
    std::array<Complex, kFftSize> data{};
    for (std::size_t i = 0; i < 16; ++i) data[i] = {static_cast<Sample>(i % 5) - 2, 0};
    auto copy = data;
    fft.forward(data.data());
    const double pi = std::acos(-1.0);
    for (std::size_t k : {0u, 1u, 7u, 100u, 255u}) {
        double re = 0, im = 0;
        for (std::size_t n = 0; n < kFftSize; ++n) {
            const double a = -2.0 * pi * static_cast<double>(k * n) / kFftSize;
            re += copy[n].re * std::cos(a);
            im += copy[n].re * std::sin(a);
        }
        EXPECT_NEAR(data[k].re, re, 1e-3);
        EXPECT_NEAR(data[k].im, im, 1e-3);
    }
}
