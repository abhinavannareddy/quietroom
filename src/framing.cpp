#include "quietroom/framing.hpp"

#include <cmath>

namespace quietroom {

OverlapAdd::OverlapAdd() : window_{}, previous_hop_{}, tail_{} {
    const double pi = std::acos(-1.0);
    for (std::size_t n = 0; n < kWindow; ++n) {
        // Periodic Hann, square-rooted.
        const double hann = 0.5 * (1.0 - std::cos(2.0 * pi * static_cast<double>(n) /
                                                  static_cast<double>(kWindow)));
        window_[n] = static_cast<Sample>(std::sqrt(hann));
    }
}

void OverlapAdd::analyze(const Sample* hop, Sample* windowed_frame) {
    for (std::size_t n = 0; n < kHop; ++n) {
        windowed_frame[n] = previous_hop_[n] * window_[n];
        windowed_frame[kHop + n] = hop[n] * window_[kHop + n];
        previous_hop_[n] = hop[n];
    }
}

void OverlapAdd::synthesize(const Sample* frame, Sample* out_hop) {
    for (std::size_t n = 0; n < kHop; ++n) {
        out_hop[n] = tail_[n] + frame[n] * window_[n];
        tail_[n] = frame[kHop + n] * window_[kHop + n];
    }
}

void OverlapAdd::reset() {
    previous_hop_.fill(0);
    tail_.fill(0);
}

}  // namespace quietroom
