#include "quietroom/vad.hpp"

#include <cmath>

namespace quietroom {

namespace {
constexpr double kEnergyEps = 1e-10;
constexpr double kPowerEps = 1e-12;
constexpr std::size_t kFlatnessLoBin = 4;    // 125 Hz
constexpr std::size_t kFlatnessHiBin = 128;  // 4 kHz
constexpr Sample kFloorFallRate = 0.2f;      // floor drops fast, rises slowly
// If the decision has said "speech" for this long without a break, the floor is almost
// certainly too low (e.g. the stream began with digital silence), so let it climb quickly.
constexpr std::size_t kStuckFrames = 300;  // 3 s
constexpr Sample kStuckRiseDb = 0.5f;
}  // namespace

Sample frame_energy_db(const Sample* hop) {
    double sum = 0;
    for (std::size_t i = 0; i < kHop; ++i) sum += static_cast<double>(hop[i]) * hop[i];
    return static_cast<Sample>(10.0 * std::log10(sum / static_cast<double>(kHop) + kEnergyEps));
}

Sample zero_crossing_rate(const Sample* hop) {
    std::size_t crossings = 0;
    for (std::size_t i = 1; i < kHop; ++i) {
        if ((hop[i] >= 0) != (hop[i - 1] >= 0)) ++crossings;
    }
    return static_cast<Sample>(crossings) / static_cast<Sample>(kHop - 1);
}

Sample spectral_flatness(const Sample* power) {
    double log_sum = 0, sum = 0;
    for (std::size_t k = kFlatnessLoBin; k < kFlatnessHiBin; ++k) {
        const double p = static_cast<double>(power[k]) + kPowerEps;
        log_sum += std::log(p);
        sum += p;
    }
    const double n = static_cast<double>(kFlatnessHiBin - kFlatnessLoBin);
    return static_cast<Sample>(std::exp(log_sum / n) / (sum / n));
}

Vad::Vad(const Config& config) : config_(config) {}

void Vad::reset() {
    floor_db_ = 0;
    has_floor_ = false;
    hangover_left_ = 0;
    speech_run_ = 0;
}

bool Vad::update(const Sample* hop, const Sample* power) {
    const Sample energy = frame_energy_db(hop);
    if (!has_floor_) {
        floor_db_ = energy;
        has_floor_ = true;
    } else if (energy < floor_db_) {
        floor_db_ += kFloorFallRate * (energy - floor_db_);
    } else {
        floor_db_ += speech_run_ > kStuckFrames ? kStuckRiseDb : config_.vad_floor_rise_db;
    }

    const bool above = energy > floor_db_ + config_.vad_margin_db;
    const bool strong = energy > floor_db_ + config_.vad_strong_margin_db;
    const bool noise_like = spectral_flatness(power) > config_.vad_flatness_max &&
                            zero_crossing_rate(hop) > config_.vad_zcr_min;
    const bool raw_speech = strong || (above && !noise_like);

    bool speech = raw_speech;
    if (raw_speech) {
        hangover_left_ = config_.hangover_frames;
    } else if (hangover_left_ > 0) {
        --hangover_left_;
        speech = true;
    }
    speech_run_ = speech ? speech_run_ + 1 : 0;
    return speech;
}

}  // namespace quietroom
