#include <gtest/gtest.h>

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
