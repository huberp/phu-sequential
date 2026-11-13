#include "arpeggiator/BlockClock.h"

namespace phu::arpeggiator {

using namespace sequencing;

SampleOffset BlockClock::toSampleOffset(Beats beat) const noexcept {
    return beatToSampleOffset(beat, blockStartBeat, samplesPerBeat, numSamples);
}

Beats BlockClock::toBeat(SampleOffset offset) const noexcept {
    return blockStartBeat + samplesToBeats(static_cast<double>(offset), samplesPerBeat);
}

bool BlockClock::contains(Beats beat) const noexcept {
    return beat >= windowStartBeat && beat < windowEndBeat;
}

} // namespace phu::arpeggiator
