#pragma once

#include <cstddef>

#include "quietroom/types.hpp"

namespace quietroom {

// All tunable parameters (F8). Defaults were tuned on the clips in data/ and python/evaluate.py.
struct Config {
    // Spectral subtraction.
    Sample over_subtraction = 2.0f;  // alpha: how much of the noise estimate to remove
    Sample spectral_floor = 0.1f;    // beta: minimum remaining fraction of the noisy power
    Sample gain_smoothing = 0.6f;    // 0 = no smoothing, closer to 1 = slower gain changes

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
};

}  // namespace quietroom
