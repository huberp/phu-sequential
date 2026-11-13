#include "arpeggiator/LanePosition.h"
#include "sequencing/MusicalTime.h"
#include "sequencing/Constants.h"
#include <cmath>

namespace phu::arpeggiator {

using namespace sequencing;
using namespace sequencing::constants;

LanePosition derivePosition(
    Beats playheadBeat,
    Beats phaseOffsetBeat,
    const GridLayout& layout) noexcept {
    if (layout.isEmpty()) {
        return LanePosition{};
    }

    const auto loopLengthBeats = layout.loopLengthBeats();
    const auto positionInLoop = playheadBeat - phaseOffsetBeat;
    const auto loopIndex = static_cast<std::int64_t>(std::floor(positionInLoop / loopLengthBeats));
    const auto wrappedPosition = positionInLoop - loopIndex * loopLengthBeats;

    const auto positionTicks = static_cast<TickCount>(wrappedPosition * timing::ticksPerBeat + 0.5);
    const auto stepIndex = stepIndexAtTick(layout, positionTicks);

    const auto stepStartTicks = layout.stepStartTicks[static_cast<std::size_t>(stepIndex)];
    const auto stepStartBeatInLoop = ticksToBeats(stepStartTicks);
    const auto stepStartBeatAbsolute = phaseOffsetBeat + loopIndex * loopLengthBeats + stepStartBeatInLoop;

    const auto isAtStart = std::abs(playheadBeat - stepStartBeatAbsolute) <= timing::beatEpsilon;

    return LanePosition{loopIndex, stepIndex, isAtStart};
}

Beats stepStartBeat(
    Beats phaseOffsetBeat,
    std::int64_t loopIndex,
    int stepIndex,
    const GridLayout& layout) noexcept {
    if (layout.isEmpty() || stepIndex < 0 || stepIndex >= layout.stepCount) {
        return phaseOffsetBeat;
    }
    const auto stepStartTicks = layout.stepStartTicks[static_cast<std::size_t>(stepIndex)];
    const auto loopLengthBeats = layout.loopLengthBeats();
    return phaseOffsetBeat + loopIndex * loopLengthBeats + ticksToBeats(stepStartTicks);
}

Beats reanchorPhaseOffset(
    Beats commitBeat,
    int newStepIndex,
    const GridLayout& newLayout) noexcept {
    if (newLayout.isEmpty() || newStepIndex < 0 || newStepIndex >= newLayout.stepCount) {
        return commitBeat;
    }
    const auto stepStartTicks = newLayout.stepStartTicks[static_cast<std::size_t>(newStepIndex)];
    const auto stepStartBeatInLoop = ticksToBeats(stepStartTicks);
    return commitBeat - stepStartBeatInLoop;
}

} // namespace phu::arpeggiator
