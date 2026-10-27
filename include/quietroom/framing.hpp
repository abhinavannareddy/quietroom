#pragma once

#include <array>

#include "quietroom/types.hpp"

namespace quietroom {

// 50% overlap framing with a sqrt-Hann window used for both analysis and synthesis.
// Because w^2 is a periodic Hann window, overlapped synthesis sums to exactly 1 and an
// untouched signal comes back delayed by one hop (kHop samples).
class OverlapAdd {
   public:
    OverlapAdd();

    // Builds the windowed 320-sample frame [previous hop | hop] from a new 160-sample hop.
    void analyze(const Sample* hop, Sample* windowed_frame);

    // Windows a processed 320-sample frame and overlap-adds it; writes one finished hop.
    void synthesize(const Sample* frame, Sample* out_hop);

    void reset();
    const Sample* window() const { return window_.data(); }

   private:
    std::array<Sample, kWindow> window_;
    std::array<Sample, kHop> previous_hop_;
    std::array<Sample, kHop> tail_;
};

}  // namespace quietroom
