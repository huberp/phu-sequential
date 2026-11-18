#pragma once

#include "arpeggiator/LaneState.h"

#include <juce_data_structures/juce_data_structures.h>

namespace phu::plugin {

struct LaneStateLoadResult {
    arpeggiator::LaneState state;
    bool clamped = false;  // some field was invalid and clamped/defaulted
    bool valid = true;     // false: not a Lane tree, state is the sec. 20 default
};

// One "Lane" tree, identical to the child that StateSerializer writes per lane (sec. 13, 22). [MT]
[[nodiscard]] juce::ValueTree laneToValueTree(const arpeggiator::LaneState&, int laneIndex);

// Never throws; missing/invalid fields fall back to defaults or are clamped. Versioning and migration act on the
// enclosing state tree (StateSerializer::fromValueTree), not on a single lane tree.
[[nodiscard]] LaneStateLoadResult laneFromValueTree(const juce::ValueTree&, int laneIndex);

}  // namespace phu::plugin
