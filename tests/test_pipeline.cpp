#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <string>
#include <vector>

#include "quietroom/pipeline.hpp"
#include "signals.hpp"

using namespace quietroom;

namespace {
std::vector<Sample> run(Pipeline& p, const std::vector<Sample>& in,
                        std::vector<bool>* speech = nullptr) {
    std::vector<Sample> out(in.size());
    for (std::size_t h = 0; h + kHop <= in.size(); h += kHop) {
        p.process_frame(&in[h], &out[h]);
        if (speech) speech->push_back(p.last_frame_was_speech());
    }
    return out;
}
}  // namespace

TEST(Pipeline, BypassReconstructsInputThroughFftAndOverlapAdd) {
    Config cfg;
    cfg.bypass = true;
    Pipeline p(cfg);
    const auto clip = testsig::make_speechlike(3.0, 1);
    const auto y = run(p, clip.samples);
    for (std::size_t n = kHop; n < y.size(); ++n) {
        ASSERT_NEAR(y[n], clip.samples[n - kHop], 1e-4) << n;
    }
}

TEST(Pipeline, ImprovesSnrAtFiveDbInput) {
    const auto clip = testsig::make_speechlike(8.0, 2);
    const auto noisy =
        testsig::mix(clip.samples, testsig::white_noise(clip.samples.size(), 5), 5.0);
    Pipeline p{Config{}};
    const auto y = run(p, noisy);
    const double in_snr = testsig::snr_db(clip.samples, noisy, 0);
    const double out_snr = testsig::snr_db(clip.samples, y, kHop);
    RecordProperty("snr_improvement_db", std::to_string(out_snr - in_snr));
    EXPECT_GT(out_snr - in_snr, 3.0) << "in=" << in_snr << " out=" << out_snr;
}

TEST(Pipeline, ConfigChangesOutput) {
    const auto clip = testsig::make_speechlike(3.0, 2);
    const auto noisy =
        testsig::mix(clip.samples, testsig::white_noise(clip.samples.size(), 5), 5.0);
    Config a, b;
    b.over_subtraction = a.over_subtraction * 0.5f;  // any value different from the default
    Pipeline pa(a), pb(b);
    EXPECT_NE(run(pa, noisy), run(pb, noisy));
}

TEST(Pipeline, MostlyFlagsSpeechHopsOnSyntheticClip) {
    const auto clip = testsig::make_speechlike(8.0, 4);
    const auto noisy =
        testsig::mix(clip.samples, testsig::white_noise(clip.samples.size(), 9), 10.0);
    Pipeline p{Config{}};
    std::vector<bool> flags;
    run(p, noisy, &flags);
    std::size_t correct = 0;
    for (std::size_t i = 0; i < flags.size(); ++i) correct += (flags[i] == clip.speech_hop[i]);
    EXPECT_GT(static_cast<double>(correct) / static_cast<double>(flags.size()), 0.85);
}

TEST(Pipeline, NonFiniteInputDoesNotPoisonLaterFrames) {
    const auto clip = testsig::make_speechlike(5.0, 6);
    auto noisy = testsig::mix(clip.samples, testsig::white_noise(clip.samples.size(), 8), 10.0);
    constexpr std::size_t kBadHop = 100;  // 1 s in, speech is still to come
    noisy[kBadHop * kHop + 3] = std::numeric_limits<Sample>::quiet_NaN();
    noisy[kBadHop * kHop + 7] = std::numeric_limits<Sample>::infinity();

    Pipeline p{Config{}};
    std::vector<bool> flags;
    const auto y = run(p, noisy, &flags);
    for (std::size_t i = 0; i < y.size(); ++i) ASSERT_TRUE(std::isfinite(y[i])) << "sample " << i;

    std::size_t speech_after = 0;
    for (std::size_t h = kBadHop + 5; h < flags.size(); ++h) speech_after += flags[h];
    EXPECT_GT(speech_after, 50u) << "the VAD stopped detecting speech after the bad samples";
}

TEST(Pipeline, OutOfRangeConfigIsClampedInsteadOfDiverging) {
    Config c;
    c.gain_smoothing = 1.5f;
    c.spectral_floor = 2.0f;
    c.over_subtraction = -1.0f;
    c.noise_smoothing = 3.0f;
    Pipeline p(c);
    EXPECT_LT(p.config().gain_smoothing, 1.0f);
    EXPECT_LE(p.config().spectral_floor, 1.0f);
    EXPECT_GE(p.config().over_subtraction, 0.0f);
    EXPECT_LT(p.config().noise_smoothing, 1.0f);

    const auto clip = testsig::make_speechlike(3.0, 2);
    const auto noisy =
        testsig::mix(clip.samples, testsig::white_noise(clip.samples.size(), 5), 5.0);
    for (Sample v : run(p, noisy)) ASSERT_LT(std::abs(v), 10.0f);
}
