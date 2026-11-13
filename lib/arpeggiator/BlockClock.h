#pragma once

#include "sequencing/MusicalTime.h"
#include "sequencing/SampleConversion.h"
#include <cstdint>

namespace phu::arpeggiator {

struct BlockClock {
    sequencing::Beats blockStartBeat = 0;
    sequencing::Beats windowStartBeat = 0;
    sequencing::Beats windowEndBeat = 0;
    double samplesPerBeat = 0;
    int numSamples = 0;

    [[nodiscard]] sequencing::SampleOffset toSampleOffset(sequencing::Beats beat) const noexcept;
    [[nodiscard]] sequencing::Beats toBeat(sequencing::SampleOffset offset) const noexcept;
    [[nodiscard]] bool contains(sequencing::Beats beat) const noexcept;
};

} // namespace phu::arpeggiator
