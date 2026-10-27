// Synthetic test signals shared by the unit tests (no external data needed).
#pragma once

#include <algorithm>
#include <cmath>
#include <random>
#include <vector>

#include "quietroom/types.hpp"

namespace testsig {

using quietroom::kHop;
using quietroom::Sample;

struct Clip {
    std::vector<Sample> samples;
    std::vector<bool> speech_hop;  // ground truth per 10 ms hop
};

inline double pi() { return std::acos(-1.0); }

// Speech-like clip: 0.5 s of silence, then voiced "words" (harmonic series, gliding pitch,
// raised-cosine envelope) separated by silent gaps. Silence is exactly zero.
inline Clip make_speechlike(double seconds, unsigned seed) {
    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> gap(0.3, 0.6), word(0.3, 0.8), f0(100.0, 180.0);
    const std::size_t total = static_cast<std::size_t>(seconds * 16000.0) / kHop * kHop;
    Clip c;
    c.samples.assign(total, 0);
    c.speech_hop.assign(total / kHop, false);
    std::size_t pos = static_cast<std::size_t>(0.5 * 16000.0);
    while (pos < total) {
        const std::size_t len = static_cast<std::size_t>(word(rng) * 16000.0);
        const double base = f0(rng);
        double phase = 0;
        for (std::size_t i = 0; i < len && pos + i < total; ++i) {
            const double t = static_cast<double>(i) / static_cast<double>(len);
            const double env = 0.5 * (1.0 - std::cos(2.0 * pi() * t));
            phase += 2.0 * pi() * base * (1.0 + 0.15 * t) / 16000.0;
            double s = 0;
            for (int h = 1; h <= 12; ++h) s += std::sin(h * phase) / h;
            c.samples[pos + i] = static_cast<Sample>(0.25 * env * s);
        }
        const std::size_t first = pos / kHop;
        const std::size_t last = std::min(c.speech_hop.size(), (pos + len) / kHop);
        for (std::size_t h = first; h < last; ++h) c.speech_hop[h] = true;
        pos += len + static_cast<std::size_t>(gap(rng) * 16000.0);
    }
    return c;
}

inline std::vector<Sample> white_noise(std::size_t n, unsigned seed) {
    std::mt19937 rng(seed);
    std::normal_distribution<float> d(0.0f, 1.0f);
    std::vector<Sample> v(n);
    for (auto& s : v) s = static_cast<Sample>(d(rng));
    return v;
}

inline double energy(const std::vector<Sample>& v) {
    double e = 0;
    for (Sample s : v) e += static_cast<double>(s) * s;
    return e;
}

// clean + noise scaled so that 10*log10(E_clean / E_noise) == snr_db.
inline std::vector<Sample> mix(const std::vector<Sample>& clean, const std::vector<Sample>& noise,
                               double snr_db) {
    const double g = std::sqrt(energy(clean) / (energy(noise) * std::pow(10.0, snr_db / 10.0)));
    std::vector<Sample> out(clean.size());
    for (std::size_t i = 0; i < clean.size(); ++i) {
        out[i] = clean[i] + static_cast<Sample>(g * noise[i]);
    }
    return out;
}

// SNR of `est` against `ref`, where est lags ref by `delay` samples.
inline double snr_db(const std::vector<Sample>& ref, const std::vector<Sample>& est,
                     std::size_t delay) {
    double num = 0, den = 0;
    for (std::size_t i = 0; i + delay < est.size() && i < ref.size(); ++i) {
        const double r = ref[i], e = est[i + delay];
        num += r * r;
        den += (e - r) * (e - r);
    }
    return 10.0 * std::log10(num / den);
}

}  // namespace testsig
