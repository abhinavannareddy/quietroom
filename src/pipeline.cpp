#include "quietroom/pipeline.hpp"

namespace quietroom {

Pipeline::Pipeline(const Config& config)
    : config_(config),
      fft_(),
      ola_(),
      vad_(config),
      noise_(config),
      subtractor_(config),
      frame_{},
      spectrum_{},
      power_{},
      gain_{} {}

void Pipeline::reset() {
    ola_.reset();
    vad_.reset();
    noise_.reset();
    subtractor_.reset();
    frames_ = 0;
    last_was_speech_ = false;
}

void Pipeline::process_frame(const Sample* in, Sample* out) noexcept {
    // 1. Window previous + current hop, zero-pad to 512 and transform.
    ola_.analyze(in, frame_.data());
    for (std::size_t i = 0; i < kWindow; ++i) spectrum_[i] = {frame_[i], 0};
    for (std::size_t i = kWindow; i < kFftSize; ++i) spectrum_[i] = {0, 0};
    fft_.forward(spectrum_.data());
    for (std::size_t k = 0; k < kBins; ++k) {
        power_[k] = spectrum_[k].re * spectrum_[k].re + spectrum_[k].im * spectrum_[k].im;
    }

    // 2. VAD. The start-up frames are assumed to be noise only.
    const bool vad_speech = vad_.update(in, power_.data());
    last_was_speech_ = frames_ >= config_.noise_init_frames && vad_speech;

    // 3. Noise estimate.
    noise_.update(power_.data(), last_was_speech_);

    // 4. Spectral subtraction, applied symmetrically so the inverse FFT stays real.
    if (!config_.bypass) {
        subtractor_.apply(power_.data(), noise_.estimate(), gain_.data());
        for (std::size_t k = 0; k < kBins; ++k) {
            spectrum_[k].re *= gain_[k];
            spectrum_[k].im *= gain_[k];
        }
        for (std::size_t k = kBins; k < kFftSize; ++k) {
            spectrum_[k].re *= gain_[kFftSize - k];
            spectrum_[k].im *= gain_[kFftSize - k];
        }
    }

    // 5. Inverse FFT, window again and overlap-add.
    fft_.inverse(spectrum_.data());
    for (std::size_t i = 0; i < kWindow; ++i) frame_[i] = spectrum_[i].re;
    ola_.synthesize(frame_.data(), out);
    ++frames_;
}

}  // namespace quietroom
