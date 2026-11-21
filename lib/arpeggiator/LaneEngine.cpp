#include "arpeggiator/LaneEngine.h"
#include "arpeggiator/LaneConfiguration.h"
#include "arpeggiator/LanePosition.h"
#include "arpeggiator/BlockClock.h"
#include "sequencing/Constants.h"
#include "sequencing/MusicalTime.h"
#include "sequencing/StepTiming.h"
#include "sequencing/StepData.h"
#include "sequencing/sources/SourceSelection.h"
#include "sequencing/velocity/VelocityModeRegistry.h"
#include "sequencing/VoiceRegistry.h"
#include "sequencing/OutputEventBuffer.h"
#include "sequencing/HeldNoteTracker.h"
#include <limits>

namespace phu::arpeggiator {

using namespace sequencing;
using namespace sequencing::constants;

LaneEngine::LaneEngine(OwnerId owner) noexcept : owner_(owner) {}

void LaneEngine::reset() noexcept {
    requested_ = LaneParameters{};
    committedSelector_ = SourceSelector{};
    committedLayout_ = GridLayout{};
    requestedLayout_ = GridLayout{};
    selectorPending_ = false;
    layoutPending_ = false;
    resynchronizeRequested_ = false;
    running_ = false;
    phaseOffsetBeat_ = 0;
    nextStepStart_ = NextStepStart{};
    currentStepIndex_ = -1;
    lastObservedSource_.reset();
    latchMemory_.reset();
    pattern_ = nullptr;
    currentDocumentRevision_ = 0;
    discardRevision_ = 0;
    discardedAtRevision_.fill(0);
}

SourceSelector LaneEngine::effectiveCommittedSelector(const LaneContext& context) const noexcept {
    SourceSelector selector = committedSelector_;
    selector.chordInputChannel = context.chordInputChannel;  // global, immediate (D-24)
    return selector;
}

void LaneEngine::commitPendingSelector() noexcept {
    committedSelector_.inputMode = requested_.inputMode;
    committedSelector_.followIndex = requested_.followIndex;
    committedSelector_.followChannel = requested_.followChannel;
    selectorPending_ = false;
}

void LaneEngine::recordDiscardIfDecrease(const GridLayout& oldLayout, const GridLayout& newLayout) noexcept {
    if (oldLayout.isEmpty() || newLayout.isEmpty() || newLayout.stepCount >= oldLayout.stepCount) {
        return;
    }
    ++discardRevision_;
    for (int slot = newLayout.stepCount; slot < lanes::maximumStepCount; ++slot) {
        discardedAtRevision_[static_cast<std::size_t>(slot)] = currentDocumentRevision_;
    }
}

void LaneEngine::commitPendingLayoutWithoutReanchor() noexcept {
    recordDiscardIfDecrease(committedLayout_, requestedLayout_);
    committedLayout_ = requestedLayout_;
    layoutPending_ = false;
}

void LaneEngine::updateSettings(
    const LaneParameters& params,
    const LanePattern& pattern,
    LaneContext& context) noexcept {
    const bool oldMute = requested_.mute;
    const bool oldLatch = requested_.latch;

    requested_ = params;
    pattern_ = &pattern;

    const bool muteTurnedOn = !oldMute && params.mute;
    const bool latchTurnedOff = oldLatch && !params.latch;
    const bool latchTurnedOn = !oldLatch && params.latch;

    if (muteTurnedOn || latchTurnedOff) {
        // Immediate (D-21): release lane voices and clear latch memory (sec. 4.6).
        context.voices.releaseOwner(owner_, 0, context.output);
        latchMemory_.reset();
    } else if (latchTurnedOn) {
        const SourceSelector selector = effectiveCommittedSelector(context);
        const auto current = selectSource(context.heldNotes, selector);
        if (current.has_value()) {
            latchMemory_ = current;
        }
    }

    // Selector (inputMode/followIndex/followChannel): next step start, unless not running (D-18).
    // chordInputChannel itself is global/immediate (D-24): folded in via effectiveCommittedSelector.
    const SourceSelector requestedSelector = makeSourceSelector(requested_, context.chordInputChannel);
    const bool selectorChanged =
        requestedSelector.inputMode != committedSelector_.inputMode ||
        requestedSelector.followIndex != committedSelector_.followIndex ||
        requestedSelector.followChannel != committedSelector_.followChannel;
    if (selectorChanged) {
        selectorPending_ = true;
        if (!running_) {
            commitPendingSelector();
        }
    }

    // Grid layout (timingSource/baseTimingIndex/subdivision/stepCount/appliedTextGrid, D-10):
    // next step start with re-anchoring (sec. 5.4), unless not running, or running with an
    // empty committed layout (D-9: commit now and ask the core to resynchronize).
    requestedLayout_ = makeActiveGridLayout(requested_, pattern_->appliedTextGrid);
    const bool layoutChanged = !(requestedLayout_ == committedLayout_);
    if (layoutChanged) {
        layoutPending_ = true;
        if (!running_) {
            commitPendingLayoutWithoutReanchor();
        } else if (committedLayout_.isEmpty()) {
            commitPendingLayoutWithoutReanchor();
            resynchronizeRequested_ = true;
        }
    }
}

bool LaneEngine::resynchronizeRequested() const noexcept {
    return resynchronizeRequested_;
}

void LaneEngine::anchorTransportStart(Beats playheadBeat) noexcept {
    phaseOffsetBeat_ = playheadBeat;
    running_ = false;
    nextStepStart_ = NextStepStart{playheadBeat, 0, 0};
    currentStepIndex_ = -1;
}

void LaneEngine::startTransport(Beats playheadBeat) noexcept {
    phaseOffsetBeat_ = playheadBeat;
    running_ = true;
    nextStepStart_ = NextStepStart{playheadBeat, 0, 0};
    currentStepIndex_ = -1;
    resynchronizeRequested_ = false;
}

void LaneEngine::stop(LaneContext& context) noexcept {
    // Commit pending changes WITHOUT re-anchoring (D-18); phase offset kept.
    if (layoutPending_) {
        commitPendingLayoutWithoutReanchor();
    }
    if (selectorPending_) {
        commitPendingSelector();
    }
    context.voices.releaseOwner(owner_, 0, context.output);
    latchMemory_.reset();
    running_ = false;
    resynchronizeRequested_ = false;
}

void LaneEngine::resynchronize(
    Beats playheadBeat,
    SampleOffset offset,
    bool releaseVoices,
    LaneContext& context) noexcept {
    if (releaseVoices) {
        context.voices.releaseOwner(owner_, offset, context.output);
    }

    // Commit pending changes WITHOUT re-anchoring (D-19); phase offset kept.
    if (layoutPending_) {
        commitPendingLayoutWithoutReanchor();
    }
    if (selectorPending_) {
        commitPendingSelector();
    }

    resynchronizeRequested_ = false;
    running_ = true;

    if (committedLayout_.isEmpty()) {
        nextStepStart_ = NextStepStart{std::numeric_limits<Beats>::infinity(), 0, 0};
        return;
    }

    // Derive position from the playhead (sec. 10), then the first step whose EFFECTIVE (swung)
    // start is >= playheadBeat - beatEpsilon (D-13, D-4).
    const LanePosition position = derivePosition(playheadBeat, phaseOffsetBeat_, committedLayout_);
    std::int64_t loopIndex = position.loopIndex;
    int stepIndex = position.stepIndex;
    for (;;) {
        const Beats gridStart = stepStartBeat(phaseOffsetBeat_, loopIndex, stepIndex, committedLayout_);
        const Beats advance = swingAdvanceBeats(
            committedLayout_.stepLengthTicks[static_cast<std::size_t>(stepIndex)],
            stepIndex, requested_.swingPercent, committedLayout_.swingApplies);
        const Beats effectiveBeat = gridStart - advance;
        if (effectiveBeat >= playheadBeat - timing::beatEpsilon) {
            nextStepStart_ = NextStepStart{effectiveBeat, loopIndex, stepIndex};
            break;
        }
        ++stepIndex;
        if (stepIndex >= committedLayout_.stepCount) {
            stepIndex = 0;
            ++loopIndex;
        }
    }
}

void LaneEngine::onHeldNotesChanged(SampleOffset offset, LaneContext& context) noexcept {
    const SourceSelector selector = effectiveCommittedSelector(context);
    const auto current = selectSource(context.heldNotes, selector);
    const auto change = classifySourceChange(lastObservedSource_, current);
    lastObservedSource_ = current;

    if (!running_) {
        return;
    }

    if (requested_.latch && !requested_.mute && current.has_value()) {
        latchMemory_ = current;  // D-22
    }

    if (change == SourceChange::released && !requested_.latch) {
        context.voices.releaseOwner(owner_, offset, context.output);
    }
}

Beats LaneEngine::nextTimelineBeat() const noexcept {
    if (!running_ || committedLayout_.isEmpty()) {
        return std::numeric_limits<Beats>::infinity();
    }
    return nextStepStart_.effectiveBeat;
}

void LaneEngine::triggerStep(int stepIndex, Beats triggerBeat, LaneContext& context) noexcept {
    if (requested_.mute) {
        return;
    }

    const SourceSelector selector = effectiveCommittedSelector(context);
    auto note = selectSource(context.heldNotes, selector);
    if (!note.has_value()) {
        if (requested_.latch && latchMemory_.has_value()) {
            note = latchMemory_;
        } else {
            return;  // nothing to trigger; position still advances (sec. 4.5).
        }
    }

    if (requested_.latch) {
        latchMemory_ = note;
    }

    const auto slot = static_cast<std::size_t>(stepIndex);
    const bool masked = discardedAtRevision_[slot] != 0 && pattern_->stepEditRevision[slot] <= discardedAtRevision_[slot];
    const StepData stepData = masked ? defaultStepData() : pattern_->steps.step(stepIndex);
    if (!stepData.enabled) {
        return;
    }

    const int modeIndex = static_cast<int>(requested_.velocityMode);
    const int stepValue = stepData.valueByMode[static_cast<std::size_t>(modeIndex)];
    const auto velocity = computeOutputVelocity(requested_.velocityMode, stepValue, note->velocity);
    if (!velocity.has_value()) {
        return;
    }

    const int pitch = static_cast<int>(note->pitch) + requested_.transpose;
    if (pitch < midi::lowestPitch || pitch > midi::highestPitch) {
        return;
    }

    const TickCount stepLengthTicks = committedLayout_.stepLengthTicks[slot];
    const Beats scheduledEnd = triggerBeat + gatedLengthBeats(stepLengthTicks, requested_.gatePercent);
    const VoiceStartRequest request{owner_, requested_.outputChannel, pitch, static_cast<int>(*velocity), scheduledEnd};
    (void)context.voices.startVoice(request, context.clock.toSampleOffset(triggerBeat), context.output);
}

void LaneEngine::scheduleNextStepStart(int stepIndex, std::int64_t loopIndex) noexcept {
    int nextStep = stepIndex + 1;
    std::int64_t nextLoop = loopIndex;
    if (nextStep >= committedLayout_.stepCount) {
        nextStep = 0;
        ++nextLoop;
    }
    const Beats gridStart = stepStartBeat(phaseOffsetBeat_, nextLoop, nextStep, committedLayout_);
    const int swing = requested_.swingPercent;  // read now, D-50
    const Beats advance = swingAdvanceBeats(
        committedLayout_.stepLengthTicks[static_cast<std::size_t>(nextStep)],
        nextStep, swing, committedLayout_.swingApplies);
    nextStepStart_ = NextStepStart{gridStart - advance, nextLoop, nextStep};
}

void LaneEngine::processNextTimelineEvent(LaneContext& context) noexcept {
    const Beats triggerBeat = nextStepStart_.effectiveBeat;
    int stepIndex = nextStepStart_.stepIndex;
    std::int64_t loopIndex = nextStepStart_.loopIndex;

    if (layoutPending_) {
        const int newStepCount = requestedLayout_.stepCount;
        const int reanchoredStepIndex = newStepCount > 0 ? (stepIndex % newStepCount) : 0;
        const Beats newPhaseOffset = newStepCount > 0
            ? reanchorPhaseOffset(triggerBeat, reanchoredStepIndex, requestedLayout_)
            : phaseOffsetBeat_;

        recordDiscardIfDecrease(committedLayout_, requestedLayout_);
        committedLayout_ = requestedLayout_;
        phaseOffsetBeat_ = newPhaseOffset;
        stepIndex = reanchoredStepIndex;
        loopIndex = 0;
        layoutPending_ = false;

        if (committedLayout_.isEmpty()) {
            // No grid: release lane voices at T, no further step starts (D-9).
            context.voices.releaseOwner(owner_, context.clock.toSampleOffset(triggerBeat), context.output);
            currentStepIndex_ = -1;
            return;
        }
    }

    if (selectorPending_) {
        commitPendingSelector();
        const SourceSelector selector = effectiveCommittedSelector(context);
        const auto current = selectSource(context.heldNotes, selector);
        const auto change = classifySourceChange(lastObservedSource_, current);
        lastObservedSource_ = current;
        if (change == SourceChange::released && !requested_.latch) {
            context.voices.releaseOwner(owner_, context.clock.toSampleOffset(triggerBeat), context.output);
        }
    }

    currentStepIndex_ = stepIndex;
    triggerStep(stepIndex, triggerBeat, context);
    scheduleNextStepStart(stepIndex, loopIndex);
}

LaneStatus LaneEngine::status() const noexcept {
    LaneStatus st;
    st.currentStepIndex = currentStepIndex_;
    st.activeStepCount = committedLayout_.isEmpty() ? 0 : committedLayout_.stepCount;
    st.isRunning = running_;
    st.hasLatchedNote = latchMemory_.has_value();
    return st;
}

void LaneEngine::onPatternAdopted(const LanePattern& pattern, std::uint64_t documentRevision) noexcept {
    currentDocumentRevision_ = documentRevision;
    if (pattern.acknowledgedDiscardRevision >= discardRevision_) {
        discardedAtRevision_.fill(0);
    }
}

std::uint32_t LaneEngine::discardRevision() const noexcept {
    return discardRevision_;
}

const std::array<std::uint64_t, constants::lanes::maximumStepCount>& LaneEngine::discardMask() const noexcept {
    return discardedAtRevision_;
}

} // namespace phu::arpeggiator
