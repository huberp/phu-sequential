#include "arpeggiator/LaneConfiguration.h"
#include "sequencing/StepTiming.h"
#include "sequencing/NoteValues.h"
#include <algorithm>

namespace phu::arpeggiator {

using namespace sequencing;
using namespace sequencing::constants;

LaneParameters clampLaneParameters(const LaneParameters& params) noexcept {
    LaneParameters clamped = params;
    clamped.followIndex = std::clamp(params.followIndex, lanes::minimumFollowIndex, lanes::maximumFollowIndex);
    clamped.followChannel = std::clamp(params.followChannel, midi::firstChannel, midi::lastChannel);
    clamped.outputChannel = std::clamp(params.outputChannel, midi::firstChannel, midi::lastChannel);
    clamped.transpose = std::clamp(params.transpose, lanes::minimumTranspose, lanes::maximumTranspose);
    clamped.gatePercent = std::clamp(params.gatePercent, lanes::minimumGatePercent, lanes::maximumGatePercent);
    clamped.swingPercent = std::clamp(params.swingPercent, lanes::minimumSwingPercent, lanes::maximumSwingPercent);
    clamped.stepCount = std::clamp(params.stepCount, lanes::minimumStepCount, lanes::maximumStepCount);
    const auto baseTimingSize = baseTimingRegistry().size();
    clamped.baseTimingIndex = std::clamp(params.baseTimingIndex, 0, static_cast<int>(baseTimingSize) - 1);
    return clamped;
}

GridLayout makeActiveGridLayout(
    const LaneParameters& params,
    const TextGrid& appliedTextGrid) noexcept {
    if (params.timingSource == TimingSource::textGrid) {
        return makeGridLayout(appliedTextGrid.activeEntries());
    }
    const auto noteLength = baseTimingNoteLength(params.baseTimingIndex, params.subdivision);
    return makeUniformGridLayout(noteLength, params.stepCount);
}

int effectiveStepCount(
    const LaneParameters& params,
    const TextGrid& appliedTextGrid) noexcept {
    if (params.timingSource == TimingSource::textGrid) {
        return appliedTextGrid.entryCount;
    }
    return params.stepCount;
}

SourceSelector makeSourceSelector(
    const LaneParameters& params,
    int chordInputChannel) noexcept {
    SourceSelector selector;
    selector.inputMode = params.inputMode;
    selector.followIndex = params.followIndex;
    selector.followChannel = params.followChannel;
    selector.chordInputChannel = chordInputChannel;
    return selector;
}

} // namespace phu::arpeggiator
