// Google Benchmark targets. BM_ProcessFrame times every frame individually with a monotonic
// clock and reports mean, p50, p99, max, deadline misses and real-time factor as counters,
// so the JSON output carries the whole latency distribution (not just an average).
#include <benchmark/benchmark.h>

#include <algorithm>
#include <chrono>
#include <vector>

#include "quietroom/fft.hpp"
#include "quietroom/pipeline.hpp"
#include "signals.hpp"

using namespace quietroom;

namespace {

constexpr double kDeadlineUs = 10000.0;  // one hop
constexpr double kBudgetUs = 2000.0;     // internal budget

std::vector<Sample> noisy_input() {
    const auto clip = testsig::make_speechlike(10.0, 11);
    return testsig::mix(clip.samples, testsig::white_noise(clip.samples.size(), 12), 5.0);
}

double percentile(const std::vector<double>& sorted, double p) {
    return sorted[static_cast<std::size_t>(p * static_cast<double>(sorted.size() - 1) + 0.5)];
}

void run_frames(benchmark::State& state, const Config& config) {
    const auto input = noisy_input();
    const std::size_t frames = input.size() / kHop;
    std::vector<Sample> out(kHop);
    Pipeline pipeline(config);
    std::vector<double> times_us;
    times_us.reserve(1 << 20);

    std::size_t i = 0;
    for (auto _ : state) {
        const auto t0 = std::chrono::steady_clock::now();
        pipeline.process_frame(&input[i * kHop], out.data());
        const auto t1 = std::chrono::steady_clock::now();
        benchmark::DoNotOptimize(out.data());
        const std::chrono::duration<double> dt = t1 - t0;
        state.SetIterationTime(dt.count());
        times_us.push_back(dt.count() * 1e6);
        i = (i + 1) % frames;
    }

    if (times_us.empty()) return;
    std::sort(times_us.begin(), times_us.end());
    double sum = 0;
    std::size_t over2 = 0, over10 = 0;
    for (double t : times_us) {
        sum += t;
        over2 += t > kBudgetUs;
        over10 += t > kDeadlineUs;
    }
    const double n = static_cast<double>(times_us.size());
    state.counters["mean_us"] = sum / n;
    state.counters["p50_us"] = percentile(times_us, 0.50);
    state.counters["p99_us"] = percentile(times_us, 0.99);
    state.counters["max_us"] = times_us.back();
    state.counters["over_2ms"] = static_cast<double>(over2);
    state.counters["over_10ms"] = static_cast<double>(over10);
    state.counters["rtf"] = (sum / n) / kDeadlineUs;
}

}  // namespace

static void BM_ProcessFrame(benchmark::State& state) { run_frames(state, Config{}); }
BENCHMARK(BM_ProcessFrame)->UseManualTime()->Repetitions(5)->Unit(benchmark::kMicrosecond);

static void BM_ProcessFrame_Bypass(benchmark::State& state) {
    Config config;
    config.bypass = true;
    run_frames(state, config);
}
BENCHMARK(BM_ProcessFrame_Bypass)->UseManualTime()->Repetitions(3)->Unit(benchmark::kMicrosecond);

static void BM_Fft512(benchmark::State& state) {
    Fft fft;
    std::vector<Complex> data(kFftSize);
    for (std::size_t i = 0; i < kFftSize; ++i) data[i] = {static_cast<Sample>(i % 7), 0};
    for (auto _ : state) {
        fft.forward(data.data());
        benchmark::DoNotOptimize(data.data());
    }
}
BENCHMARK(BM_Fft512)->Unit(benchmark::kMicrosecond);

BENCHMARK_MAIN();
