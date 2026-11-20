#pragma once

#include "arpeggiator/LaneConfiguration.h"
#include "arpeggiator/PatternDocument.h"

namespace phu::arpeggiator {

// Canonical per-lane value (sec. 22): the host-parameter side (M12) plus the pattern side (M21).
// Pure C++, no JUCE. Copies are deep value copies; operator== compares every property.
struct LaneState {
    LaneParameters configuration;
    LaneDocument document;

    friend bool operator==(const LaneState&, const LaneState&) = default;
};

// Sec. 20 defaults of lane laneIndex (index clamped).
[[nodiscard]] LaneState defaultLaneState(int laneIndex);

}  // namespace phu::arpeggiator
