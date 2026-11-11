#pragma once

#include "sequencing/Constants.h"
#include "sequencing/MusicalTime.h"
#include <cmath>
#include <cstdint>

// The single beats->samples boundary (§10, §16.2, §17). Pure, header-only.
namespace phu::sequencing {

using SampleOffset = std::int32_t;

// sampleRate * 60 / bpm; returns 0 if bpm <= 0 or sampleRate <= 0 (caller treats as "no clock").
[[nodiscard]] constexpr double samplesPerBeat(double tempoBeatsPerMinute, double sampleRate) noexcept {
    if (tempoBeatsPerMinute <= 0.0 || sampleRate <= 0.0) {
        return 0.0;
    }
    return sampleRate * constants::timing::secondsPerMinute / tempoBeatsPerMinute;
}

[[nodiscard]] constexpr double beatsToSamples(Beats duration, double samplesPerBeat) noexcept {
    return duration * samplesPerBeat;
}

[[nodiscard]] constexpr Beats samplesToBeats(double samples, double samplesPerBeat) noexcept {
    if (samplesPerBeat == 0.0) {
        return 0.0;
    }
    return samples / samplesPerBeat;
}

// D-15: floor((beat - blockStartBeat) * samplesPerBeat + sampleRoundingEpsilon), clamped to [0, numSamples-1]
// (returns 0 when numSamples <= 0).
[[nodiscard]] SampleOffset beatToSampleOffset(Beats beat, Beats blockStartBeat,
                                              double samplesPerBeat, int numSamples) noexcept {
    if (numSamples <= 0) {
        return 0;
    }
    const double rawOffset = (beat - blockStartBeat) * samplesPerBeat + constants::timing::sampleRoundingEpsilon;
    const double flooredOffset = std::floor(rawOffset);
    const double clampedOffset = flooredOffset < 0.0
        ? 0.0
        : (flooredOffset > static_cast<double>(numSamples - 1) ? static_cast<double>(numSamples - 1) : flooredOffset);
    return static_cast<SampleOffset>(clampedOffset);
}

} // namespace phu::sequencing
