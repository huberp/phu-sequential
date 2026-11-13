#pragma once

#include "TransportTracker.h"
#include "sequencing/OutputEventBuffer.h"
#include <cstdint>

namespace phu::arpeggiator {

struct BlockInput {
    std::span<const sequencing::MidiEvent> events;
    TransportInfo transport;
    double sampleRate = 0;
    int numSamples = 0;
    bool bypassed = false;
    bool moreInputAtSameOffset = false;
};

struct LaneDiscardState {
    std::uint32_t discardRevision = 0;
    std::array<std::uint64_t, sequencing::constants::lanes::maximumStepCount> discardedAtRevision{};
};

using StepDiscardState = std::array<LaneDiscardState, sequencing::constants::lanes::laneCount>;

struct BlockResult {
    bool discardStateChanged = false;
    std::uint32_t droppedOutputEvents = 0;
};

}
