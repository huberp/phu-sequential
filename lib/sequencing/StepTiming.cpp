#include "sequencing/StepTiming.h"
#include <algorithm>

namespace phu::sequencing {

namespace {

[[nodiscard]] int clampInt(int value, int lo, int hi) noexcept {
    return value < lo ? lo : (value > hi ? hi : value);
}

} // namespace

Beats GridLayout::loopLengthBeats() const noexcept {
    return ticksToBeats(loopLengthTicks());
}

bool operator==(const GridLayout& lhs, const GridLayout& rhs) noexcept {
    if (lhs.stepCount != rhs.stepCount || lhs.swingApplies != rhs.swingApplies) {
        return false;
    }
    for (int i = 0; i < lhs.stepCount; ++i) {
        if (lhs.stepLengthTicks[static_cast<std::size_t>(i)] != rhs.stepLengthTicks[static_cast<std::size_t>(i)]) {
            return false;
        }
    }
    return true;
}

namespace {

[[nodiscard]] GridLayout buildGridLayout(std::span<const TickCount> lengths, bool swingApplies) noexcept {
    GridLayout layout;
    layout.stepCount = static_cast<int>(lengths.size());
    layout.swingApplies = swingApplies;
    TickCount cumulative = 0;
    layout.stepStartTicks[0] = 0;
    for (std::size_t i = 0; i < lengths.size(); ++i) {
        layout.stepLengthTicks[i] = lengths[i];
        cumulative += lengths[i];
        layout.stepStartTicks[i + 1] = cumulative;
    }
    return layout;
}

} // namespace

GridLayout makeUniformGridLayout(NoteLength noteLength, int stepCount) noexcept {
    const int clampedStepCount = clampInt(stepCount, constants::lanes::minimumStepCount, constants::lanes::maximumStepCount);
    const TickCount lengthTicks = noteLengthTicks(noteLength);
    std::array<TickCount, constants::lanes::maximumStepCount> lengths{};
    for (int i = 0; i < clampedStepCount; ++i) {
        lengths[static_cast<std::size_t>(i)] = lengthTicks;
    }
    return buildGridLayout(std::span<const TickCount>(lengths.data(), static_cast<std::size_t>(clampedStepCount)), true);
}

GridLayout makeGridLayout(std::span<const NoteLength> entries) noexcept {
    const std::size_t count = std::min(entries.size(), static_cast<std::size_t>(constants::lanes::maximumStepCount));
    std::array<TickCount, constants::lanes::maximumStepCount> lengths{};
    for (std::size_t i = 0; i < count; ++i) {
        lengths[i] = noteLengthTicks(entries[i]);
    }
    return buildGridLayout(std::span<const TickCount>(lengths.data(), count), false);
}

Beats swingAdvanceBeats(TickCount stepLengthTicks, int stepIndexInLoop, int swingPercent, bool swingApplies) noexcept {
    if (!swingApplies || (stepIndexInLoop % 2) == 0) {
        return 0.0;
    }
    const int clampedSwingPercent = clampInt(swingPercent, constants::lanes::minimumSwingPercent, constants::lanes::maximumSwingPercent);
    return ticksToBeats(stepLengthTicks) * clampedSwingPercent / static_cast<double>(constants::lanes::swingPercentToShiftDivisor);
}

Beats gatedLengthBeats(TickCount stepLengthTicks, int gatePercent) noexcept {
    const int clampedGatePercent = clampInt(gatePercent, constants::lanes::minimumGatePercent, constants::lanes::maximumGatePercent);
    return ticksToBeats(stepLengthTicks) * clampedGatePercent / static_cast<double>(constants::lanes::gatePercentDivisor);
}

int stepIndexAtTick(const GridLayout& layout, TickCount positionInLoop) noexcept {
    if (layout.stepCount <= 0) {
        return 0;
    }
    // Largest k with stepStartTicks[k] <= positionInLoop, i.e. upper_bound - 1.
    const auto begin = layout.stepStartTicks.begin();
    const auto end = begin + layout.stepCount + 1;
    const auto it = std::upper_bound(begin, end, positionInLoop);
    const int index = static_cast<int>(std::distance(begin, it)) - 1;
    return clampInt(index, 0, layout.stepCount - 1);
}

} // namespace phu::sequencing
