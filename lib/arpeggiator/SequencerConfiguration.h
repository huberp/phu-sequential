#pragma once

#include "LaneConfiguration.h"
#include "sequencing/Constants.h"
#include <array>

namespace phu::arpeggiator {

struct PatternSnapshot {
    std::array<LanePattern, sequencing::constants::lanes::laneCount> lanes{};
    std::uint64_t documentRevision = 0;
};

struct SequencerConfiguration {
    GlobalParameters global;
    std::array<LaneParameters, sequencing::constants::lanes::laneCount> lanes;

    friend constexpr bool operator==(const SequencerConfiguration&, const SequencerConfiguration&) noexcept = default;
};

[[nodiscard]] SequencerConfiguration defaultSequencerConfiguration() noexcept;
[[nodiscard]] SequencerConfiguration clampConfiguration(const SequencerConfiguration&) noexcept;

}
