#include "SequencerCore.h"
#include "sequencing/MidiEvent.h"

#include <algorithm>
#include <limits>
#include <span>

namespace phu::arpeggiator {

using namespace sequencing;

namespace {

constexpr Beats infiniteBeat = std::numeric_limits<Beats>::infinity();

// A timeline item at `beat` belongs to this block iff its sample offset, computed with the exact
// arithmetic BlockClock::toSampleOffset uses (D-15) but before the final clamp, is < numSamples.
// This mirrors "tItem < clock.windowEndBeat" (arch. 5.2) but stays consistent with the actual
// sample mapping even under the sub-ulp floating-point drift that TransportTracker's windowEndBeat
// accumulates across many continuous blocks (D-16): comparing beats directly against windowEndBeat
// can disagree, by a single sample, with what toSampleOffset(beat) would later compute relative to
// blockStartBeat, which would otherwise clamp such an item onto the wrong (last) sample of this
// block instead of correctly deferring it to sample 0 of the next one (REQ-10-13 block-size
// invariance).
[[nodiscard]] bool dueWithinBlock(Beats beat, const BlockClock& clock) noexcept {
    if (clock.numSamples <= 0) {
        return false;
    }
    const double raw = (beat - clock.blockStartBeat) * clock.samplesPerBeat +
                       sequencing::constants::timing::sampleRoundingEpsilon;
    return raw < static_cast<double>(clock.numSamples);
}

}  // namespace

SequencerCore::SequencerCore() noexcept
    : lanes_{LaneEngine(0), LaneEngine(1), LaneEngine(2), LaneEngine(3),
             LaneEngine(4), LaneEngine(5), LaneEngine(6), LaneEngine(7)} {
    for (int i = 0; i < sequencing::constants::lanes::laneCount; ++i) {
        voices_.setVoiceLimit(static_cast<OwnerId>(i), sequencing::constants::lanes::maximumVoicesPerLane);
    }
    voices_.setVoiceLimit(passThroughOwner, 0);  // 0 = unlimited (sec. 9.4 limits lanes only)
}

void SequencerCore::reset() noexcept {
    // [prepare, audio stopped]: only marks a pending flush; emits nothing here (D-44). The next
    // processBlock releases all sounding voices at offset 0, then resets all state to stopped.
    flushPending_ = true;
}

void SequencerCore::setPatterns(const PatternSnapshot& snapshot) noexcept {
    if (snapshot.documentRevision < patterns_.documentRevision) {
        return;  // stale snapshot, ignored
    }
    patterns_ = snapshot;
    for (int i = 0; i < sequencing::constants::lanes::laneCount; ++i) {
        const auto lane = static_cast<std::size_t>(i);
        lanes_[lane].onPatternAdopted(patterns_.lanes[lane], patterns_.documentRevision);
    }
}

BlockResult SequencerCore::processBlock(const BlockInput& input, const SequencerConfiguration& configuration,
                                        sequencing::OutputEventBuffer& output) noexcept {
    BlockResult result{};

    if (input.numSamples == 0) {
        // D-53: nothing emitted, no state change; everything pending stays pending.
        return result;
    }

    output.clear();

    const SequencerConfiguration cfg = clampConfiguration(configuration);
    const int previousChordInputChannel = configuration_.global.chordInputChannel;
    configuration_ = cfg;
    const auto laneSpan = std::span<const LaneParameters, sequencing::constants::lanes::laneCount>(cfg.lanes);
    const PassThroughPolicy policy = makePassThroughPolicy(cfg.global, laneSpan);

    if (flushPending_) {
        voices_.releaseAll(0, output);
        voices_.reset();
        passThrough_.reset();
        heldNotes_.reset();
        transport_.reset();
        for (auto& lane : lanes_) {
            lane.reset();
        }
        flushPending_ = false;
    }

    // 1. Transport and clock (always first, D-14).
    const TransportUpdate update = transport_.update(input.transport, input.sampleRate, input.numSamples,
                                                      !input.moreInputAtSameOffset);
    const BlockClock& clock = update.clock;
    transportPlaying_ = update.transition != TransportTransition::idle &&
                        update.transition != TransportTransition::stopped;
    LaneContext ctx{voices_, output, heldNotes_, clock, cfg.global.chordInputChannel};

    std::array<std::uint32_t, sequencing::constants::lanes::laneCount> discardRevisionBefore{};
    for (int i = 0; i < sequencing::constants::lanes::laneCount; ++i) {
        discardRevisionBefore[static_cast<std::size_t>(i)] = lanes_[static_cast<std::size_t>(i)].discardRevision();
    }

    // 2. Bypass (sec. 21, D-27).
    const bool enteringBypass = input.bypassed && !wasBypassed_;
    if (enteringBypass) {
        voices_.releaseAll(0, output);
        passThrough_.clearTracking();
        for (auto& lane : lanes_) {
            lane.stop(ctx);
        }
    }

    bool resyncAfterBypass = false;
    if (input.bypassed) {
        if (update.transition == TransportTransition::started) {
            for (auto& lane : lanes_) {
                lane.anchorTransportStart(clock.blockStartBeat);
            }
        }
        for (const auto& event : input.events) {
            const MidiEvent normalized = normalizeNoteEvent(event);
            heldNotes_.apply(normalized);
            passThrough_.trackRawNote(normalized);
            if (event.kind == MidiEventKind::noteOn || event.kind == MidiEventKind::noteOff) {
                // Forward raw, unchanged (sec. 21); CC64 and other non-note events are copied by
                // the adapter, not the core (D-31).
                if (event.isNoteRelease()) {
                    output.pushNoteOff(event);
                } else {
                    (void)output.tryPushNoteOn(event);
                }
            }
        }
        wasBypassed_ = true;
        result.droppedOutputEvents = output.droppedEventCount();
        droppedTotal_ += result.droppedOutputEvents;
        return result;
    }

    if (wasBypassed_) {
        // Exiting bypass (D-27): adopt raw-held keys as pass-through voices (no output), then
        // resync lanes from the current playhead without resetting to step 1 (REQ-21-4).
        passThrough_.adoptRawNotes(voices_);
        wasBypassed_ = false;
        resyncAfterBypass = true;
    }

    // 3. Settings.
    for (int i = 0; i < sequencing::constants::lanes::laneCount; ++i) {
        const auto lane = static_cast<std::size_t>(i);
        lanes_[lane].updateSettings(cfg.lanes[lane], patterns_.lanes[lane], ctx);
    }

    // 4. Transitions.
    switch (update.transition) {
        case TransportTransition::stopped:
            voices_.releaseAll(0, output);
            passThrough_.clearTracking();
            for (auto& lane : lanes_) {
                lane.stop(ctx);
            }
            break;
        case TransportTransition::started:
            for (auto& lane : lanes_) {
                lane.startTransport(clock.blockStartBeat);
            }
            break;
        case TransportTransition::discontinuity:
            voices_.releaseAll(0, output);
            passThrough_.clearTracking();
            for (auto& lane : lanes_) {
                lane.resynchronize(clock.blockStartBeat, 0, /*releaseVoices=*/false, ctx);
            }
            break;
        case TransportTransition::continuous:
            for (auto& lane : lanes_) {
                if (resyncAfterBypass || lane.resynchronizeRequested()) {
                    lane.resynchronize(clock.windowStartBeat, 0, /*releaseVoices=*/false, ctx);
                }
            }
            break;
        case TransportTransition::idle:
            break;
    }

    const bool chordInputChannelChanged = cfg.global.chordInputChannel != previousChordInputChannel;
    if (chordInputChannelChanged && transportPlaying_) {
        for (auto& lane : lanes_) {
            lane.onHeldNotesChanged(0, ctx);
        }
    }

    // 5. Merged loop: input events x voice ends x lane step starts, in time order.
    std::size_t eventIndex = 0;
    for (;;) {
        const std::optional<ScheduledVoiceEnd> voiceEnd = voices_.nextScheduledEnd();
        const Beats tv = voiceEnd.has_value() ? voiceEnd->beat : infiniteBeat;

        int laneIndex = -1;
        Beats tl = infiniteBeat;
        for (int i = 0; i < sequencing::constants::lanes::laneCount; ++i) {
            const Beats laneBeat = lanes_[static_cast<std::size_t>(i)].nextTimelineBeat();
            if (laneBeat < tl) {
                tl = laneBeat;
                laneIndex = i;
            }
        }

        bool timelineItemExists = false;
        bool timelineIsVoiceEnd = false;
        SampleOffset sItem = 0;

        if (input.moreInputAtSameOffset) {
            // D-51: non-final piece of an oversized same-offset group; lane step starts are
            // suppressed, and only a voice end that is due exactly at offset 0 may be processed.
            if (voiceEnd.has_value() && dueWithinBlock(tv, clock)) {
                const SampleOffset candidateOffset = clock.toSampleOffset(tv);
                if (candidateOffset == 0) {
                    timelineItemExists = true;
                    timelineIsVoiceEnd = true;
                    sItem = candidateOffset;
                }
            }
        } else {
            const Beats tItem = std::min(tv, tl);
            if (dueWithinBlock(tItem, clock)) {
                timelineItemExists = true;
                timelineIsVoiceEnd = tv <= tl;
                sItem = clock.toSampleOffset(tItem);
            }
        }

        const bool hasInputEvent = eventIndex < input.events.size();
        const SampleOffset eventOffset = hasInputEvent ? input.events[eventIndex].sampleOffset : 0;

        if (!hasInputEvent && !timelineItemExists) {
            break;
        }

        bool processInputFirst = false;
        if (hasInputEvent) {
            if (!timelineItemExists) {
                processInputFirst = true;
            } else if (eventOffset < sItem) {
                processInputFirst = true;
            } else if (eventOffset == sItem && !timelineIsVoiceEnd) {
                // D-14.4: input event first when the timeline item is a lane event.
                processInputFirst = true;
            }
        }

        if (processInputFirst) {
            const MidiEvent& rawEvent = input.events[eventIndex];
            const MidiEvent normalized = normalizeNoteEvent(rawEvent);
            const bool changed = heldNotes_.apply(normalized);
            passThrough_.process(normalized, policy, voices_, output);
            if (changed && transportPlaying_) {
                for (auto& lane : lanes_) {
                    lane.onHeldNotesChanged(rawEvent.sampleOffset, ctx);
                }
            }
            ++eventIndex;
        } else if (timelineIsVoiceEnd) {
            voices_.endVoice(voiceEnd->voice, sItem, output);
        } else {
            lanes_[static_cast<std::size_t>(laneIndex)].processNextTimelineEvent(ctx);
        }
    }

    // 6. Publish.
    for (int i = 0; i < sequencing::constants::lanes::laneCount; ++i) {
        if (lanes_[static_cast<std::size_t>(i)].discardRevision() != discardRevisionBefore[static_cast<std::size_t>(i)]) {
            result.discardStateChanged = true;
            break;
        }
    }
    result.droppedOutputEvents = output.droppedEventCount();
    droppedTotal_ += result.droppedOutputEvents;
    return result;
}

PlaybackStatus SequencerCore::playbackStatus() const noexcept {
    PlaybackStatus status{};
    for (int i = 0; i < sequencing::constants::lanes::laneCount; ++i) {
        status.lanes[static_cast<std::size_t>(i)] = lanes_[static_cast<std::size_t>(i)].status();
    }
    status.transportPlaying = transportPlaying_;
    status.bypassed = wasBypassed_;
    status.droppedOutputEventsTotal = droppedTotal_;
    return status;
}

StepDiscardState SequencerCore::stepDiscardState() const noexcept {
    StepDiscardState state{};
    for (int i = 0; i < sequencing::constants::lanes::laneCount; ++i) {
        const auto lane = static_cast<std::size_t>(i);
        state[lane].discardRevision = lanes_[lane].discardRevision();
        state[lane].discardedAtRevision = lanes_[lane].discardMask();
    }
    return state;
}

}
