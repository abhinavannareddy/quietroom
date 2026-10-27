// quietroom_cli in.wav out.wav [options]
// Reads a 16 kHz mono WAV, removes noise frame by frame and writes the result. Prints
// per-frame compute time and VAD statistics. Output is aligned with the input (the pipeline's
// one-hop delay is removed here).
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

#include "quietroom/pipeline.hpp"
#include "quietroom/wav.hpp"

using namespace quietroom;

namespace {

struct Options {
    std::string input;
    std::string output;
    std::string vad_csv;
    std::string json;
    Config config;
};

void usage() {
    std::fprintf(stderr,
                 "usage: quietroom_cli in.wav out.wav [options]\n"
                 "  --over-subtraction X   noise scale alpha, >= 0        (default 3.0)\n"
                 "  --floor X              spectral floor beta, 0..1      (default 0.02)\n"
                 "  --smoothing X          gain smoothing, 0..<1          (default 0.3)\n"
                 "  --hangover N           VAD hangover frames            (default 8)\n"
                 "  --bypass               FFT/IFFT only, no denoising\n"
                 "  --vad-csv FILE         write one 0/1 VAD decision per 10 ms frame\n"
                 "  --json FILE            write timing and VAD statistics as JSON\n");
}

bool parse_number(const char* text, double lo, double hi, double& out) {
    char* end = nullptr;
    out = std::strtod(text, &end);
    return end != text && *end == '\0' && out >= lo && out <= hi;
}

bool parse_args(int argc, char** argv, Options& opt) {
    std::vector<std::string> positional;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto value = [&](double lo, double hi, double& v) {
            if (i + 1 >= argc || !parse_number(argv[i + 1], lo, hi, v)) {
                std::fprintf(stderr, "error: %s needs a number in [%g, %g]\n", a.c_str(), lo, hi);
                return false;
            }
            ++i;
            return true;
        };
        double v = 0;
        if (a == "--over-subtraction") {
            if (!value(0, 20, v)) return false;
            opt.config.over_subtraction = static_cast<Sample>(v);
        } else if (a == "--floor") {
            if (!value(0, 1, v)) return false;
            opt.config.spectral_floor = static_cast<Sample>(v);
        } else if (a == "--smoothing") {
            if (!value(0, 0.999, v)) return false;
            opt.config.gain_smoothing = static_cast<Sample>(v);
        } else if (a == "--hangover") {
            if (!value(0, 1000, v)) return false;
            opt.config.hangover_frames = static_cast<std::size_t>(v);
        } else if (a == "--bypass") {
            opt.config.bypass = true;
        } else if (a == "--vad-csv" || a == "--json") {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "error: %s needs a file name\n", a.c_str());
                return false;
            }
            (a == "--vad-csv" ? opt.vad_csv : opt.json) = argv[++i];
        } else if (a.rfind("--", 0) == 0) {
            std::fprintf(stderr, "error: unknown option %s\n", a.c_str());
            return false;
        } else {
            positional.push_back(a);
        }
    }
    if (positional.size() != 2) return false;
    opt.input = positional[0];
    opt.output = positional[1];
    return true;
}

double percentile(std::vector<double> sorted, double p) {
    if (sorted.empty()) return 0;
    const std::size_t idx = static_cast<std::size_t>(p * static_cast<double>(sorted.size() - 1) + 0.5);
    return sorted[idx];
}

}  // namespace

int main(int argc, char** argv) {
    Options opt;
    if (!parse_args(argc, argv, opt)) {
        usage();
        return 2;
    }

    WavData wav;
    std::string error;
    if (!read_wav(opt.input, wav, error)) {
        std::fprintf(stderr, "error: %s\n", error.c_str());
        return 1;
    }

    const std::size_t length = wav.samples.size();
    const std::size_t hops = (length + kHop - 1) / kHop;
    std::vector<Sample> in((hops + 1) * kHop, 0);  // one extra zero hop flushes the pipeline
    std::copy(wav.samples.begin(), wav.samples.end(), in.begin());
    std::vector<Sample> out(in.size(), 0);

    Pipeline pipeline(opt.config);
    std::vector<double> frame_us;
    std::vector<unsigned char> decisions;
    frame_us.reserve(hops + 1);
    decisions.reserve(hops + 1);

    using Clock = std::chrono::steady_clock;
    for (std::size_t h = 0; h <= hops; ++h) {
        const auto t0 = Clock::now();
        pipeline.process_frame(&in[h * kHop], &out[h * kHop]);
        const auto t1 = Clock::now();
        frame_us.push_back(std::chrono::duration<double, std::micro>(t1 - t0).count());
        decisions.push_back(pipeline.last_frame_was_speech() ? 1 : 0);
    }

    // Output lags the input by one hop: drop it and trim to the original length.
    std::vector<float> result(length);
    for (std::size_t i = 0; i < length; ++i) result[i] = static_cast<float>(out[i + kHop]);
    if (!write_wav(opt.output, result, kSampleRate, error)) {
        std::fprintf(stderr, "error: %s\n", error.c_str());
        return 1;
    }

    // Statistics over the frames of real audio (not the flush frame).
    std::vector<double> timed(frame_us.begin(), frame_us.begin() + static_cast<long>(hops));
    std::vector<double> sorted = timed;
    std::sort(sorted.begin(), sorted.end());
    double sum = 0;
    std::size_t over2 = 0, over10 = 0, speech = 0;
    for (double t : timed) {
        sum += t;
        over2 += t > 2000.0;
        over10 += t > 10000.0;
    }
    for (std::size_t h = 0; h < hops; ++h) speech += decisions[h];
    const double mean = timed.empty() ? 0 : sum / static_cast<double>(timed.size());
    const double audio_us = static_cast<double>(hops) * 10000.0;
    const double rtf = audio_us > 0 ? sum / audio_us : 0;

    std::printf("frames: %zu (%.2f s)\n", hops, static_cast<double>(length) / kSampleRate);
    std::printf("compute per frame: mean %.1f us, p50 %.1f us, p99 %.1f us, max %.1f us\n", mean,
                percentile(sorted, 0.50), percentile(sorted, 0.99), percentile(sorted, 1.0));
    std::printf("deadline misses: %zu over 10 ms, %zu over 2 ms; real-time factor %.5f\n", over10, over2,
                rtf);
    std::printf("VAD: %zu speech frames, %zu non-speech frames\n", speech, hops - speech);

    if (!opt.vad_csv.empty()) {
        std::ofstream f(opt.vad_csv);
        if (!f) {
            std::fprintf(stderr, "error: cannot create %s\n", opt.vad_csv.c_str());
            return 1;
        }
        for (std::size_t h = 0; h < hops; ++h) f << static_cast<int>(decisions[h]) << '\n';
    }
    if (!opt.json.empty()) {
        std::ofstream f(opt.json);
        if (!f) {
            std::fprintf(stderr, "error: cannot create %s\n", opt.json.c_str());
            return 1;
        }
        f << "{\"frames\": " << hops << ", \"mean_us\": " << mean << ", \"p50_us\": " << percentile(sorted, 0.50)
          << ", \"p99_us\": " << percentile(sorted, 0.99) << ", \"max_us\": " << percentile(sorted, 1.0)
          << ", \"over_2ms\": " << over2 << ", \"over_10ms\": " << over10 << ", \"rtf\": " << rtf
          << ", \"speech_frames\": " << speech << ", \"frame_us\": [";
        for (std::size_t i = 0; i < timed.size(); ++i) f << (i ? "," : "") << timed[i];
        f << "]}\n";
    }
    return 0;
}
