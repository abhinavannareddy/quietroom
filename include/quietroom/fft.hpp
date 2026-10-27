#pragma once

#include <array>
#include <cstdint>

#include "quietroom/types.hpp"

namespace quietroom {

struct Complex {
    Sample re;
    Sample im;
};

// In-place iterative radix-2 FFT of fixed size kFftSize. Twiddle factors and the
// bit-reversal table are built once in the constructor, so transforms never allocate.
class Fft {
public:
    Fft();

    void forward(Complex* data) const;
    // Includes the 1/N scaling, so inverse(forward(x)) == x.
    void inverse(Complex* data) const;

private:
    void transform(Complex* data, bool invert) const;

    std::array<Complex, kFftSize / 2> twiddle_;  // exp(-2*pi*i*k/N)
    std::array<std::uint16_t, kFftSize> bit_reverse_;
};

}  // namespace quietroom
