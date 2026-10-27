#include <gtest/gtest.h>

#include <array>
#include <random>
#include <vector>

#include "quietroom/framing.hpp"

using namespace quietroom;

TEST(OverlapAdd, SquaredWindowsSumToOneAtHalfOverlap) {
    OverlapAdd ola;
    const Sample* w = ola.window();
    for (std::size_t n = 0; n < kHop; ++n) {
        EXPECT_NEAR(w[n] * w[n] + w[n + kHop] * w[n + kHop], 1.0, 1e-6);
    }
}

TEST(OverlapAdd, PassthroughReconstructsInputDelayedByOneHop) {
    constexpr std::size_t kHops = 60;
    std::mt19937 rng(1);
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
    std::vector<Sample> x(kHops * kHop), y(kHops * kHop);
    for (auto& v : x) v = static_cast<Sample>(dist(rng));

    OverlapAdd ola;
    std::array<Sample, kWindow> frame{};
    for (std::size_t h = 0; h < kHops; ++h) {
        ola.analyze(&x[h * kHop], frame.data());
        ola.synthesize(frame.data(), &y[h * kHop]);
    }
    // y[n] == x[n - kHop]; the first hop is a half-windowed start-up transient.
    for (std::size_t n = kHop; n < y.size(); ++n) {
        ASSERT_NEAR(y[n], x[n - kHop], 1e-5) << "n=" << n;
    }
}

TEST(OverlapAdd, ResetClearsState) {
    OverlapAdd ola;
    std::array<Sample, kHop> in{};
    in.fill(1);
    std::array<Sample, kWindow> frame{};
    std::array<Sample, kHop> out{};
    ola.analyze(in.data(), frame.data());
    ola.synthesize(frame.data(), out.data());
    ola.reset();
    in.fill(0);
    ola.analyze(in.data(), frame.data());
    ola.synthesize(frame.data(), out.data());
    for (Sample v : out) EXPECT_EQ(v, 0);
}
