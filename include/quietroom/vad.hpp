#pragma once

#include "quietroom/config.hpp"
#include "quietroom/types.hpp"

namespace quietroom {

// Per-frame features. All take a 160-sample hop (or its 257-bin power spectrum).
Sample frame_energy_db(const Sample* hop);
Sample zero_crossing_rate(const Sample* hop);  // sign changes per sample pair, 0..1
// Geometric / arithmetic mean of the power spectrum between ~125 Hz and 4 kHz, 0..1.
Sample spectral_flatness(const Sample* power);

// Voice activity detector: energy against an adaptive floor, with ZCR and spectral flatness
// used to reject weak noise-like frames, then a hangover so word endings are not clipped.
class Vad {
public:
    explicit Vad(const Config& config);

    // Returns true if the frame (including hangover) counts as speech.
    bool update(const Sample* hop, const Sample* power);

    void reset();
    Sample floor_db() const { return floor_db_; }

private:
    Config config_;
    Sample floor_db_ = 0;
    bool has_floor_ = false;
    std::size_t hangover_left_ = 0;
};

}  // namespace quietroom
