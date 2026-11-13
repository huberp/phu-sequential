#pragma once

#include "sequencing/MusicalTime.h"
#include "sequencing/StepTiming.h"
#include <cstdint>

namespace phu::arpeggiator {

struct LanePosition {
    std::int64_t loopIndex = 0;
    int stepIndex = 0;
    bool isAtStepStart = false;
};

[[nodiscard]] LanePosition derivePosition(
    sequencing::Beats playheadBeat,
    sequencing::Beats phaseOffsetBeat,
    const sequencing::GridLayout& layout) noexcept;

[[nodiscard]] sequencing::Beats stepStartBeat(
    sequencing::Beats phaseOffsetBeat,
    std::int64_t loopIndex,
    int stepIndex,
    const sequencing::GridLayout& layout) noexcept;

[[nodiscard]] sequencing::Beats reanchorPhaseOffset(
    sequencing::Beats commitBeat,
    int newStepIndex,
    const sequencing::GridLayout& newLayout) noexcept;

} // namespace phu::arpeggiator
