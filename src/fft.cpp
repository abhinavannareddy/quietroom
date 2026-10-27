#include "quietroom/fft.hpp"

#include <cmath>
#include <utility>

namespace quietroom {

Fft::Fft() : twiddle_{}, bit_reverse_{} {
    const double pi = std::acos(-1.0);
    for (std::size_t k = 0; k < kFftSize / 2; ++k) {
        const double a = -2.0 * pi * static_cast<double>(k) / static_cast<double>(kFftSize);
        twiddle_[k] = {static_cast<Sample>(std::cos(a)), static_cast<Sample>(std::sin(a))};
    }
    std::size_t bits = 0;
    while ((std::size_t{1} << bits) < kFftSize) ++bits;
    for (std::size_t i = 0; i < kFftSize; ++i) {
        std::size_t r = 0;
        for (std::size_t b = 0; b < bits; ++b) {
            if (i & (std::size_t{1} << b)) r |= std::size_t{1} << (bits - 1 - b);
        }
        bit_reverse_[i] = static_cast<std::uint16_t>(r);
    }
}

void Fft::forward(Complex* data) const { transform(data, false); }

void Fft::inverse(Complex* data) const {
    transform(data, true);
    const Sample scale = Sample{1} / static_cast<Sample>(kFftSize);
    for (std::size_t i = 0; i < kFftSize; ++i) {
        data[i].re *= scale;
        data[i].im *= scale;
    }
}

void Fft::transform(Complex* data, bool invert) const {
    for (std::size_t i = 0; i < kFftSize; ++i) {
        const std::size_t j = bit_reverse_[i];
        if (i < j) std::swap(data[i], data[j]);
    }
    for (std::size_t len = 2; len <= kFftSize; len <<= 1) {
        const std::size_t half = len / 2;
        const std::size_t stride = kFftSize / len;
        for (std::size_t start = 0; start < kFftSize; start += len) {
            for (std::size_t k = 0; k < half; ++k) {
                const Complex w = twiddle_[k * stride];
                const Sample wr = w.re;
                const Sample wi = invert ? -w.im : w.im;
                Complex& a = data[start + k];
                Complex& b = data[start + k + half];
                const Sample tr = b.re * wr - b.im * wi;
                const Sample ti = b.re * wi + b.im * wr;
                b.re = a.re - tr;
                b.im = a.im - ti;
                a.re += tr;
                a.im += ti;
            }
        }
    }
}

}  // namespace quietroom
