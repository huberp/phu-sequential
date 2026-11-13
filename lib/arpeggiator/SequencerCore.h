#pragma once

#include "BlockTypes.h"
#include "SequencerConfiguration.h"
#include "TransportTracker.h"
#include "PassThroughFilter.h"
#include "LaneEngine.h"
#include "sequencing/HeldNoteTracker.h"
#include "sequencing/VoiceRegistry.h"
#include "sequencing/OutputEventBuffer.h"
#include "sequencing/Constants.h"
#include <array>

namespace phu::arpeggiator {

struct PlaybackStatus {
    std::array<LaneStatus, sequencing::constants::lanes::laneCount> lanes{};
    bool transportPlaying = false;
    bool bypassed = false;
    std::uint32_t droppedOutputEventsTotal = 0;
    std::uint32_t hostOutputGrowthCount = 0;
};

class SequencerCore {
public:
    SequencerCore() noexcept;
    void reset() noexcept;
    void setPatterns(const PatternSnapshot&) noexcept;
    BlockResult processBlock(const BlockInput&, const SequencerConfiguration&, sequencing::OutputEventBuffer& output) noexcept;
    [[nodiscard]] PlaybackStatus playbackStatus() const noexcept;
    [[nodiscard]] StepDiscardState stepDiscardState() const noexcept;

private:
    SequencerConfiguration configuration_ = defaultSequencerConfiguration();
    PatternSnapshot patterns_{};
    sequencing::HeldNoteTracker heldNotes_;
    sequencing::VoiceRegistry voices_;
    PassThroughFilter passThrough_;
    TransportTracker transport_;
    std::array<LaneEngine, sequencing::constants::lanes::laneCount> lanes_;
    bool wasBypassed_ = false;
    bool flushPending_ = false;
    std::uint32_t droppedTotal_ = 0;
    // Not part of the architecture.md sketch: playbackStatus() has no other way to report the
    // transport's playing state once the call that observed it has returned (D-20).
    bool transportPlaying_ = false;
};

}
