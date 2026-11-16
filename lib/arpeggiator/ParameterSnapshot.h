#pragma once

#include "sequencing/Constants.h"
#include <array>

namespace phu::arpeggiator {

// Raw values exactly as read from the host parameters, before validation (sec. 12, 17).
// Bools are 0/1, choices are 0-based indices, ints are rounded integers. Defaults = sec. 20.
struct RawLaneParameterValues {
    int mute = 0;
    int latch = 0;
    int inputMode = 0;
    int followIndex = sequencing::constants::lanes::defaultFollowIndex;
    int followChannel = sequencing::constants::lanes::defaultFollowChannel;
    int outputChannel = sequencing::constants::lanes::defaultOutputChannel;
    int transpose = sequencing::constants::lanes::defaultTranspose;
    int gate = sequencing::constants::lanes::defaultGatePercent;
    int timingSource = 0;
    int baseTiming = sequencing::constants::timing::defaultBaseTimingIndex;
    int subdivision = 0;
    int swing = sequencing::constants::lanes::defaultSwingPercent;
    int stepCount = sequencing::constants::lanes::defaultStepCount;
    int velocityMode = 0;
};

struct ParameterSnapshot {
    int thru = 0;
    int consume = 1;
    int chordInputChannel = sequencing::constants::lanes::defaultChordInputChannel;
    std::array<RawLaneParameterValues, sequencing::constants::lanes::laneCount> lanes{};
};

} // namespace phu::arpeggiator
