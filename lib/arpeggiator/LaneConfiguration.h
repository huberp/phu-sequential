#pragma once

#include "sequencing/Constants.h"
#include "sequencing/MusicalTime.h"
#include "sequencing/NoteValues.h"
#include "sequencing/StepTiming.h"
#include "sequencing/TextGridParser.h"
#include "sequencing/sources/SourceSelection.h"
#include "sequencing/velocity/VelocityMode.h"
#include "sequencing/StepData.h"
#include <array>
#include <cstdint>

namespace phu::arpeggiator {

enum class TimingSource : std::uint8_t { baseTiming = 0, textGrid = 1 };
inline constexpr int timingSourceCount = 2;

struct TimingSourceDefinition {
    std::string_view name;
    std::string_view persistenceKey;
};

inline constexpr std::array<TimingSourceDefinition, timingSourceCount> timingSourceRegistry{{
    {"Base Timing", "baseTiming"}, {"Text Grid", "textGrid"}}};

// Host parameters of one lane (sec. 12), already validated (see clampLaneParameters).
// Defaults = sec. 20.
struct LaneParameters {
    bool mute = false;
    bool latch = false;
    sequencing::InputMode inputMode = sequencing::InputMode::chordFollow;
    int followIndex = sequencing::constants::lanes::defaultFollowIndex;
    int followChannel = sequencing::constants::lanes::defaultFollowChannel;
    int outputChannel = sequencing::constants::lanes::defaultOutputChannel;
    int transpose = sequencing::constants::lanes::defaultTranspose;
    int gatePercent = sequencing::constants::lanes::defaultGatePercent;
    TimingSource timingSource = TimingSource::baseTiming;
    int baseTimingIndex = sequencing::constants::timing::defaultBaseTimingIndex;
    sequencing::Subdivision subdivision = sequencing::Subdivision::straight;
    int swingPercent = sequencing::constants::lanes::defaultSwingPercent;
    int stepCount = sequencing::constants::lanes::defaultStepCount;
    sequencing::VelocityMode velocityMode = sequencing::VelocityMode::incoming;

    friend constexpr bool operator==(const LaneParameters&, const LaneParameters&) noexcept = default;
};

struct GlobalParameters {
    bool thru = false;
    bool consume = true;
    int chordInputChannel = sequencing::constants::lanes::defaultChordInputChannel;

    friend constexpr bool operator==(const GlobalParameters&, const GlobalParameters&) noexcept = default;
};

// Non-parameter lane data the audio thread needs (sec. 12, 13).
struct LanePattern {
    sequencing::StepPattern steps;
    sequencing::TextGrid appliedTextGrid;
    std::array<std::uint64_t, sequencing::constants::lanes::maximumStepCount> stepEditRevision{};
    std::uint32_t acknowledgedDiscardRevision = 0;

    friend bool operator==(const LanePattern&, const LanePattern&) noexcept = default;
};

[[nodiscard]] LaneParameters clampLaneParameters(const LaneParameters&) noexcept;

[[nodiscard]] sequencing::GridLayout makeActiveGridLayout(
    const LaneParameters&, const sequencing::TextGrid& appliedTextGrid) noexcept;

[[nodiscard]] int effectiveStepCount(
    const LaneParameters&, const sequencing::TextGrid& appliedTextGrid) noexcept;

[[nodiscard]] sequencing::SourceSelector makeSourceSelector(
    const LaneParameters&, int chordInputChannel) noexcept;

} // namespace phu::arpeggiator
