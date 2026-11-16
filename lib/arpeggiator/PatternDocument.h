#pragma once

#include "arpeggiator/LaneConfiguration.h"
#include "arpeggiator/SequencerConfiguration.h"
#include "sequencing/Constants.h"
#include "sequencing/TextGridParser.h"
#include "sequencing/velocity/VelocityMode.h"

#include <array>
#include <cstdint>
#include <string>

namespace phu::arpeggiator {

struct LaneColor {
    std::uint8_t red = 0;
    std::uint8_t green = 0;
    std::uint8_t blue = 0;

    friend constexpr bool operator==(const LaneColor&, const LaneColor&) noexcept = default;
};

// Palette color of a lane (sec. 20); the index is clamped.
[[nodiscard]] LaneColor defaultLaneColor(int laneIndex) noexcept;

struct LaneDocument {
    LanePattern pattern;          // step data + applied (last valid) text grid
    std::string textGridText;     // exactly what the user typed, possibly invalid (sec. 5.3)
    LaneColor color;
};

// Message-thread model of all non-parameter state (sec. 12, 13). Pure C++, no JUCE. [MT]
class PatternDocument {
public:
    PatternDocument();

    void resetToDefaults();

    [[nodiscard]] const LaneDocument& lane(int laneIndex) const noexcept;  // index clamped

    void setStepEnabled(int laneIndex, int stepIndex, bool enabled);
    void setStepValue(int laneIndex, int stepIndex, sequencing::VelocityMode, int value);  // value clamped

    // The text is always stored; the applied grid changes only when the text is valid (sec. 5.3).
    sequencing::TextGridParseResult setTextGridText(int laneIndex, std::string text);
    void setLaneColor(int laneIndex, LaneColor);

    // Wholesale replacement (state loading). Revision stays monotonic.
    void setLane(int laneIndex, LaneDocument);

    // Invariant I-1: resets every step slot at or beyond the lane's effective step count to its default.
    void enforceStepCountInvariant(int laneIndex, const LaneParameters&);

    [[nodiscard]] std::uint64_t revision() const noexcept { return revision_; }
    [[nodiscard]] PatternSnapshot makeSnapshot() const;

    friend bool operator==(const PatternDocument& a, const PatternDocument& b) noexcept;

private:
    std::array<LaneDocument, sequencing::constants::lanes::laneCount> lanes_;
    std::uint64_t revision_ = 1;
};

}  // namespace phu::arpeggiator
