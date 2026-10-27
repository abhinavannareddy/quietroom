#pragma once

#include <cstddef>

namespace quietroom {

// Sample type is chosen at compile time (F9): float by default, double with -DQUIETROOM_DOUBLE=ON.
#ifdef QUIETROOM_DOUBLE
using Sample = double;
#else
using Sample = float;
#endif

inline constexpr std::size_t kSampleRate = 16000;
inline constexpr std::size_t kHop = 160;      // 10 ms
inline constexpr std::size_t kWindow = 320;   // 20 ms analysis window
inline constexpr std::size_t kFftSize = 512;  // zero-padded
inline constexpr std::size_t kBins = kFftSize / 2 + 1;

}  // namespace quietroom
