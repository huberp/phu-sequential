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

// D-15/D-60: ceil((beat - blockStartBeat) * samplesPerBeat - sampleRoundingEpsilon), i.e. the first sample at or
// after the beat, clamped to [0, numSamples-1] (returns 0 when numSamples <= 0). Rounding up keeps a host event
// for the same musical position (floored, rounded or ceiled by the host) at or before the generated event.
[[nodiscard]] inline SampleOffset beatToSampleOffset(Beats beat, Beats blockStartBeat,
                                              double samplesPerBeat, int numSamples) noexcept {
    if (numSamples <= 0) {
        return 0;
    }
    const double rawOffset = (beat - blockStartBeat) * samplesPerBeat - constants::timing::sampleRoundingEpsilon;
    const double roundedOffset = std::ceil(rawOffset);
    const double clampedOffset = roundedOffset < 0.0
        ? 0.0
        : (roundedOffset > static_cast<double>(numSamples - 1) ? static_cast<double>(numSamples - 1) : roundedOffset);
    return static_cast<SampleOffset>(clampedOffset);
}

// D-59: sample offset in a host block at which a looping host jumps from loopEndBeat back to the loop
// start: ceil((loopEndBeat - blockStartBeat) * samplesPerBeat - sampleRoundingEpsilon), i.e. the first
// sample at or after the loop end. -1 if the block does not cross the loop end inside (0, numSamples).
[[nodiscard]] inline int loopWrapSampleOffset(Beats blockStartBeat, Beats loopStartBeat, Beats loopEndBeat,
                                              double samplesPerBeat, int numSamples) noexcept {
    if (samplesPerBeat <= 0.0 || numSamples <= 1 || loopEndBeat - loopStartBeat <= constants::timing::beatEpsilon ||
        blockStartBeat >= loopEndBeat - constants::timing::beatEpsilon) {
        return -1;
    }
    const double rawOffset = (loopEndBeat - blockStartBeat) * samplesPerBeat - constants::timing::sampleRoundingEpsilon;
    const double wrap = std::ceil(rawOffset);
    return (wrap > 0.0 && wrap < static_cast<double>(numSamples)) ? static_cast<int>(wrap) : -1;
}

} // namespace phu::sequencing
