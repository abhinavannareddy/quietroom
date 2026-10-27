#pragma once

#include <cstddef>

#include "quietroom/types.hpp"

namespace quietroom {

// All tunable parameters (F8). Defaults were tuned on the clips in data/ and python/evaluate.py.
struct Config {
    // Spectral subtraction.
    Sample over_subtraction = 3.0f;  // alpha: how much of the noise estimate to remove
    Sample spectral_floor = 0.02f;   // beta: minimum remaining fraction of the noisy power
    Sample gain_smoothing = 0.3f;    // 0 = no smoothing, closer to 1 = slower gain changes

    // Noise estimate.
    std::size_t noise_init_frames = 20;  // 200 ms assumed noise-only at start
    Sample noise_smoothing = 0.95f;      // exponential smoothing on non-speech frames

    // Voice activity detection.
    std::size_t hangover_frames = 8;     // keep "speech" this many frames after it stops
    Sample vad_margin_db = 6.0f;         // energy above the tracked floor to call speech
    Sample vad_strong_margin_db = 12.0f; // above this, speech regardless of ZCR/flatness
    Sample vad_floor_rise_db = 0.02f;    // how fast the energy floor can rise per frame
    Sample vad_flatness_max = 0.5f;      // weak frames this flat and ...
    Sample vad_zcr_min = 0.3f;           // ... this busy are treated as noise

    // Pass the signal through FFT/IFFT untouched (used to test reconstruction).
    bool bypass = false;

    // The values the library actually uses: out-of-range or NaN settings are pulled back into
    // the range where the filters stay stable (smoothing >= 1 would freeze or diverge).
    Config clamped() const {
        constexpr Sample kMaxSmoothing = 0.999f;
        const auto limit = [](Sample v, Sample lo, Sample hi) { return v < lo ? lo : (v > hi ? hi : (v == v ? v : lo)); };
        Config c = *this;
        c.over_subtraction = limit(over_subtraction, 0, 100);
        c.spectral_floor = limit(spectral_floor, 0, 1);
        c.gain_smoothing = limit(gain_smoothing, 0, kMaxSmoothing);
        c.noise_smoothing = limit(noise_smoothing, 0, kMaxSmoothing);
        c.vad_margin_db = limit(vad_margin_db, 0, 100);
        c.vad_strong_margin_db = limit(vad_strong_margin_db, c.vad_margin_db, 200);
        c.vad_floor_rise_db = limit(vad_floor_rise_db, 0, 10);
        c.vad_flatness_max = limit(vad_flatness_max, 0, 1);
        c.vad_zcr_min = limit(vad_zcr_min, 0, 1);
        return c;
    }
};

}  // namespace quietroom
