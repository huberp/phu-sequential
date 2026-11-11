#pragma once

#include "sequencing/Constants.h"
#include "sequencing/MusicalTime.h"
#include "sequencing/NoteValues.h"
#include <array>
#include <span>

// Owns the grid model shared by base timing and text grid (§5.1, §5.2,
// §5.4), swing (§5.1) and gate (§6).
namespace phu::sequencing {

struct GridLayout {
    int stepCount = 0;   // 0 = no grid (text grid without valid content, D-9) -> lane silent
    std::array<TickCount, constants::lanes::maximumStepCount>     stepLengthTicks{};
    std::array<TickCount, constants::lanes::maximumStepCount + 1> stepStartTicks{}; // [stepCount] = loop length
    bool swingApplies = false;  // true only for base timing (§5.1 "swing applies only to base timing")

    [[nodiscard]] bool isEmpty() const noexcept { return stepCount == 0; }
    [[nodiscard]] TickCount loopLengthTicks() const noexcept { return stepStartTicks[static_cast<std::size_t>(stepCount)]; }
    [[nodiscard]] Beats loopLengthBeats() const noexcept;
    // Equality compares stepCount, swingApplies, and lengths of the first stepCount steps only.
    friend bool operator==(const GridLayout& lhs, const GridLayout& rhs) noexcept;
};

// Base timing (§5.1, §5.4): stepCount clamped to [1, 64]; swingApplies = true.
[[nodiscard]] GridLayout makeUniformGridLayout(NoteLength noteLength, int stepCount) noexcept;           // [RT]
// Text grid (§5.2): entries.size() <= 64; empty span -> empty layout; swingApplies = false.
[[nodiscard]] GridLayout makeGridLayout(std::span<const NoteLength> entries) noexcept;        // [RT]

// Swing (§5.1): ADVANCE (D-4: the step starts EARLIER) of steps with odd 0-based index (even 1-based);
// never step 0 of a loop. advance = stepLength * swingPercent / 200; 0 if !swingApplies or index even.
// swingPercent clamped [0,100]. Effective step start = grid start - advance.
[[nodiscard]] Beats swingAdvanceBeats(TickCount stepLengthTicks, int stepIndexInLoop,
                                      int swingPercent, bool swingApplies) noexcept;          // [RT]
// Gate (§6): stepLength * gatePercent / 100, gatePercent clamped [1,200]. Always > 0.
[[nodiscard]] Beats gatedLengthBeats(TickCount stepLengthTicks, int gatePercent) noexcept;    // [RT]
// Index of the step containing tick position p in [0, loopLength): largest k with start[k] <= p.
[[nodiscard]] int stepIndexAtTick(const GridLayout& layout, TickCount positionInLoop) noexcept;      // [RT], binary search

} // namespace phu::sequencing
