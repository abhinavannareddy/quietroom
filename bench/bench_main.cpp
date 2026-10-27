#include <benchmark/benchmark.h>

#include "quietroom/version.hpp"

static void BM_Version(benchmark::State& state) {
    for (auto _ : state) {
        benchmark::DoNotOptimize(quietroom::version());
    }
}
BENCHMARK(BM_Version);
