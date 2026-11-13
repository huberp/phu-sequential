#pragma once

#include "LaneConfiguration.h"
#include "BlockClock.h"
#include "LanePosition.h"
#include "sequencing/MusicalTime.h"
#include "sequencing/VoiceRegistry.h"
#include "sequencing/OutputEventBuffer.h"
#include "sequencing/HeldNoteTracker.h"
#include "sequencing/Constants.h"
#include <optional>
#include <array>
#include <cstdint>

namespace phu::arpeggiator {

struct LaneContext {
    sequencing::VoiceRegistry& voices;
    sequencing::OutputEventBuffer& output;
    const sequencing::HeldNoteTracker& heldNotes;
    const BlockClock& clock;
    int chordInputChannel;
};

struct LaneStatus {
    int currentStepIndex = -1;
    int activeStepCount = 0;
    bool isRunning = false;
    bool hasLatchedNote = false;
};

class LaneEngine {
public:
    explicit LaneEngine(sequencing::OwnerId owner) noexcept;
    void reset() noexcept;

    void updateSettings(
        const LaneParameters& params,
        const LanePattern& pattern,
        LaneContext& context) noexcept;

    [[nodiscard]] bool resynchronizeRequested() const noexcept;

    void anchorTransportStart(sequencing::Beats playheadBeat) noexcept;
    void startTransport(sequencing::Beats playheadBeat) noexcept;
    void stop(LaneContext& context) noexcept;
    void resynchronize(
        sequencing::Beats playheadBeat,
        sequencing::SampleOffset offset,
        bool releaseVoices,
        LaneContext& context) noexcept;

    void onHeldNotesChanged(sequencing::SampleOffset offset, LaneContext& context) noexcept;

    [[nodiscard]] sequencing::Beats nextTimelineBeat() const noexcept;
    void processNextTimelineEvent(LaneContext& context) noexcept;
    [[nodiscard]] LaneStatus status() const noexcept;

    void onPatternAdopted(const LanePattern& pattern, std::uint64_t documentRevision) noexcept;
    [[nodiscard]] std::uint32_t discardRevision() const noexcept;
    [[nodiscard]] const std::array<std::uint64_t, sequencing::constants::lanes::maximumStepCount>&
    discardMask() const noexcept;

private:
    // Implementation-detail helpers (not part of the architecture.md sketch).
    [[nodiscard]] sequencing::SourceSelector effectiveCommittedSelector(const LaneContext& context) const noexcept;
    void commitPendingSelector() noexcept;
    void commitPendingLayoutWithoutReanchor() noexcept;
    void recordDiscardIfDecrease(const sequencing::GridLayout& oldLayout,
                                  const sequencing::GridLayout& newLayout) noexcept;
    void triggerStep(int stepIndex, sequencing::Beats triggerBeat, LaneContext& context) noexcept;
    void scheduleNextStepStart(int stepIndex, std::int64_t loopIndex) noexcept;


    sequencing::OwnerId owner_;
    const LanePattern* pattern_ = nullptr;
    std::uint64_t currentDocumentRevision_ = 0;
    std::uint32_t discardRevision_ = 0;
    std::array<std::uint64_t, sequencing::constants::lanes::maximumStepCount> discardedAtRevision_{};
    LaneParameters requested_{};
    sequencing::SourceSelector committedSelector_{};
    sequencing::GridLayout committedLayout_{};
    sequencing::GridLayout requestedLayout_{};
    bool selectorPending_ = false;
    bool layoutPending_ = false;
    bool resynchronizeRequested_ = false;
    bool running_ = false;
    sequencing::Beats phaseOffsetBeat_ = 0;
    struct NextStepStart {
        sequencing::Beats effectiveBeat;
        std::int64_t loopIndex;
        int stepIndex;
    };
    NextStepStart nextStepStart_{};
    int currentStepIndex_ = -1;
    std::optional<sequencing::SourceNote> lastObservedSource_;
    std::optional<sequencing::SourceNote> latchMemory_;
};

} // namespace phu::arpeggiator
