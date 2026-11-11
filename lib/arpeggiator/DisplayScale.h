#pragma once

#include "sequencing/Constants.h"
#include "sequencing/MusicalTime.h"
#include "sequencing/StepTiming.h"
#include <array>
#include <span>

// Pure geometry for §14 (common time scale), testable without JUCE.
namespace phu::arpeggiator {

struct LaneDisplayGeometry {
    double loopWidthPixels = 0;                                         // loopLengthBeats * pixelsPerBeat
    std::array<double, sequencing::constants::lanes::maximumStepCount + 1> stepStartPixels{}; // [stepCount] = loop end
    int stepCount = 0;                                                  // 0: empty grid -> "no grid" placeholder
};

struct DisplayScale {
    double pixelsPerBeat = 0;
    double contentWidthPixels = 0;     // longest loop * pixelsPerBeat; > available -> horizontal scrolling
    std::array<LaneDisplayGeometry, sequencing::constants::lanes::laneCount> lanes{};
};

// pixelsPerBeat = availableWidth / longestLoopBeats, then raised to minimumStepDisplayWidth / shortestStepBeats
// if any step would be narrower than constants::editor::minimumStepDisplayWidth (§14). Swing ignored (§14).
// All lanes empty -> pixelsPerBeat 0, all widths 0.
[[nodiscard]] DisplayScale computeDisplayScale(
    std::span<const sequencing::GridLayout, sequencing::constants::lanes::laneCount> layouts,
    double availableWidthPixels) noexcept;

} // namespace phu::arpeggiator
