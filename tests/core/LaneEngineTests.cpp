#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "arpeggiator/LaneEngine.h"
#include "arpeggiator/LaneConfiguration.h"
#include "arpeggiator/BlockClock.h"
#include "arpeggiator/LanePosition.h"
#include "sequencing/VoiceRegistry.h"
#include "sequencing/OutputEventBuffer.h"
#include "sequencing/HeldNoteTracker.h"
#include "sequencing/MidiEvent.h"
#include "sequencing/StepData.h"
#include "sequencing/StepTiming.h"
#include "sequencing/NoteValues.h"
#include "sequencing/TextGridParser.h"
#include "sequencing/sources/InputModeRegistry.h"
#include "sequencing/velocity/VelocityMode.h"
#include "sequencing/Constants.h"
#include <limits>
#include <vector>

// M14 Lane engine: one lane's step scheduling, triggering, live changes, latch, and
// re-anchoring behavior (sec. 4.4-4.6, 5.2, 5.4, 6, 7.3, 8, 9, 11, #10).

using namespace phu::arpeggiator;
using namespace phu::sequencing;
using namespace phu::sequencing::constants;

namespace {

int countEventsOfKind(const OutputEventBuffer& buffer, MidiEventKind kind) noexcept {
    int count = 0;
    for (const auto& event : buffer.events()) {
        if (event.kind == kind) {
            ++count;
        }
    }
    return count;
}

int countEventsOfKindForPitch(const OutputEventBuffer& buffer, MidiEventKind kind, int pitch) noexcept {
    int count = 0;
    for (const auto& event : buffer.events()) {
        if (event.kind == kind && event.data1 == pitch) {
            ++count;
        }
    }
    return count;
}

// Note-On events only, in emission order: same-pitch retriggers (sec. 9.1) interleave Note-Offs
// into the raw buffer, so scenarios that only care about each triggered step's Note-On must
// filter them out first rather than index the raw buffer directly.
std::vector<MidiEvent> noteOnEventsInOrder(const OutputEventBuffer& buffer) {
    std::vector<MidiEvent> result;
    for (const auto& event : buffer.events()) {
        if (event.kind == MidiEventKind::noteOn) {
            result.push_back(event);
        }
    }
    return result;
}

// A lane fixture wiring together the non-owning services a LaneEngine needs each call,
// with a BlockClock window wide enough to cover every beat these scenario tests reach.
struct LaneFixture {
    VoiceRegistry voices;
    OutputEventBuffer output{capacity::outputEventsPerBlock};
    HeldNoteTracker held;
    BlockClock clock{};
    int chordInputChannel = 1;
    LaneEngine engine;

    explicit LaneFixture(OwnerId owner = 0) : engine(owner) {
        voices.setVoiceLimit(owner, lanes::maximumVoicesPerLane);
        clock.blockStartBeat = 0.0;
        clock.samplesPerBeat = 1000.0;
        clock.numSamples = 1'000'000'000;
        clock.windowStartBeat = clock.blockStartBeat;
        clock.windowEndBeat = clock.blockStartBeat + clock.numSamples / clock.samplesPerBeat;
    }

    LaneContext context() noexcept { return LaneContext{voices, output, held, clock, chordInputChannel}; }

    void updateSettings(const LaneParameters& params, const LanePattern& pattern) noexcept {
        auto ctx = context();
        engine.updateSettings(params, pattern, ctx);
    }

    // Drives every pending step start up to and including targetBeat.
    void runUntil(Beats targetBeat) noexcept {
        while (engine.nextTimelineBeat() <= targetBeat) {
            auto ctx = context();
            engine.processNextTimelineEvent(ctx);
        }
    }

    void setHeldNote(int channel, int pitch, int velocity, SampleOffset offset = 0) noexcept {
        held.apply(makeNoteOn(offset, channel, pitch, velocity));
        auto ctx = context();
        engine.onHeldNotesChanged(offset, ctx);
    }

    void releaseHeldNote(int channel, int pitch, SampleOffset offset = 0) noexcept {
        held.apply(makeNoteOff(offset, channel, pitch));
        auto ctx = context();
        engine.onHeldNotesChanged(offset, ctx);
    }
};

LaneParameters baseTimingParams(int stepCount, int baseTimingIndex, Subdivision subdivision = Subdivision::straight) {
    LaneParameters params;
    params.timingSource = TimingSource::baseTiming;
    params.baseTimingIndex = baseTimingIndex;
    params.subdivision = subdivision;
    params.stepCount = stepCount;
    return params;
}

}  // namespace

TEST_CASE("LaneEngine: gate percent determines the scheduled note-off beat of a triggered step",
          "[M14][REQ-6-2]") {
    LaneFixture fixture;
    auto params = baseTimingParams(/*stepCount=*/4, /*baseTimingIndex=*/1 /* 1/16 */);
    params.gatePercent = 50;
    const LanePattern pattern;

    fixture.updateSettings(params, pattern);
    fixture.engine.startTransport(0.0);
    fixture.setHeldNote(fixture.chordInputChannel, 60, 100);

    fixture.runUntil(0.0);  // trigger step 0 at beat 0

    REQUIRE(countEventsOfKindForPitch(fixture.output, MidiEventKind::noteOn, 60) == 1);
    const auto scheduledEnd = fixture.voices.nextScheduledEnd();
    REQUIRE(scheduledEnd.has_value());
    CHECK(scheduledEnd->beat == Catch::Approx(0.125));  // 1/16 step (0.25 beat) * 50 % gate
}

TEST_CASE("LaneEngine: a transposed pitch outside 0..127 is not sent, with no clamping",
          "[M14][REQ-8-4]") {
    LaneFixture fixture;
    auto params = baseTimingParams(/*stepCount=*/1, /*baseTimingIndex=*/3 /* 1/4 */);
    params.transpose = 24;
    const LanePattern pattern;

    fixture.updateSettings(params, pattern);
    fixture.engine.startTransport(0.0);
    fixture.setHeldNote(fixture.chordInputChannel, 120, 100);  // 120 + 24 = 144, out of range

    fixture.runUntil(0.0);

    CHECK(fixture.output.size() == 0);
    CHECK(fixture.voices.soundingCount() == 0);
}

TEST_CASE("LaneEngine: a transposed pitch below 0 is not sent either", "[M14][REQ-8-4]") {
    LaneFixture fixture;
    auto params = baseTimingParams(/*stepCount=*/1, /*baseTimingIndex=*/3);
    params.transpose = -24;
    const LanePattern pattern;

    fixture.updateSettings(params, pattern);
    fixture.engine.startTransport(0.0);
    fixture.setHeldNote(fixture.chordInputChannel, 5, 100);  // 5 - 24 = -19, out of range

    fixture.runUntil(0.0);

    CHECK(fixture.output.size() == 0);
}

TEST_CASE("LaneEngine: mute suppresses triggers but the step position keeps advancing",
          "[M14][REQ-4.5-3][REQ-9.3-6]") {
    LaneFixture fixture;
    auto params = baseTimingParams(/*stepCount=*/4, /*baseTimingIndex=*/3 /* 1/4 */);
    params.mute = true;
    const LanePattern pattern;

    fixture.updateSettings(params, pattern);
    fixture.engine.startTransport(0.0);
    fixture.setHeldNote(fixture.chordInputChannel, 60, 100);

    fixture.runUntil(0.0);
    CHECK(fixture.engine.status().currentStepIndex == 0);
    fixture.runUntil(1.0);
    CHECK(fixture.engine.status().currentStepIndex == 1);
    fixture.runUntil(2.0);
    CHECK(fixture.engine.status().currentStepIndex == 2);

    CHECK(fixture.output.size() == 0);  // muted: no notes were ever sent
    CHECK(fixture.voices.soundingCount() == 0);
}

TEST_CASE("LaneEngine: mute off->on releases lane voices and clears latch memory",
          "[M14][REQ-9.3-6][REQ-4.6-4]") {
    LaneFixture fixture;
    auto params = baseTimingParams(/*stepCount=*/4, /*baseTimingIndex=*/3);
    params.latch = true;
    params.gatePercent = 400;  // keep the voice sounding well past the test point
    const LanePattern pattern;

    fixture.updateSettings(params, pattern);
    fixture.engine.startTransport(0.0);
    fixture.setHeldNote(fixture.chordInputChannel, 60, 100);
    fixture.runUntil(0.0);
    REQUIRE(fixture.voices.soundingCount() == 1);
    REQUIRE(fixture.engine.status().hasLatchedNote);

    fixture.output.clear();
    params.mute = true;
    fixture.updateSettings(params, pattern);  // mute off->on: immediate release + clear latch memory

    CHECK(countEventsOfKind(fixture.output, MidiEventKind::noteOff) == 1);
    CHECK(fixture.voices.soundingCount() == 0);
    CHECK_FALSE(fixture.engine.status().hasLatchedNote);
}

TEST_CASE("LaneEngine: latch keeps the lane sounding on the last followed note after release",
          "[M14][REQ-4.6-2]") {
    LaneFixture fixture;
    auto params = baseTimingParams(/*stepCount=*/4, /*baseTimingIndex=*/3);
    params.latch = true;
    const LanePattern pattern;

    fixture.updateSettings(params, pattern);
    fixture.engine.startTransport(0.0);
    fixture.setHeldNote(fixture.chordInputChannel, 60, 100);
    fixture.runUntil(0.0);  // step 0 triggers on pitch 60, latch memory = 60
    REQUIRE(countEventsOfKindForPitch(fixture.output, MidiEventKind::noteOn, 60) == 1);

    fixture.output.clear();
    fixture.releaseHeldNote(fixture.chordInputChannel, 60);  // source now empty; latch keeps the lane alive
    CHECK(countEventsOfKind(fixture.output, MidiEventKind::noteOff) == 0);  // no immediate stop with latch on

    fixture.runUntil(1.0);  // step 1 triggers again, using latch memory (pitch 60)
    CHECK(countEventsOfKindForPitch(fixture.output, MidiEventKind::noteOn, 60) == 1);
}

TEST_CASE("LaneEngine: latch updates the followed note at the next step start, not mid-step",
          "[M14][REQ-4.6-3]") {
    LaneFixture fixture;
    auto params = baseTimingParams(/*stepCount=*/4, /*baseTimingIndex=*/3);
    params.latch = true;
    const LanePattern pattern;

    fixture.updateSettings(params, pattern);
    fixture.engine.startTransport(0.0);
    fixture.setHeldNote(fixture.chordInputChannel, 60, 100);
    fixture.runUntil(0.0);  // step 0: pitch 60

    fixture.setHeldNote(fixture.chordInputChannel, 64, 110);  // new note mid-step
    fixture.releaseHeldNote(fixture.chordInputChannel, 60);
    fixture.releaseHeldNote(fixture.chordInputChannel, 64);  // now fully released; latch should hold pitch 64

    fixture.output.clear();
    fixture.runUntil(1.0);  // step 1: latch memory should now be 64 (last followed note)
    CHECK(countEventsOfKindForPitch(fixture.output, MidiEventKind::noteOn, 64) == 1);
    CHECK(countEventsOfKindForPitch(fixture.output, MidiEventKind::noteOn, 60) == 0);
}

TEST_CASE("LaneEngine: switching latch off releases lane voices and clears latch memory",
          "[M14][REQ-4.6-4]") {
    LaneFixture fixture;
    auto params = baseTimingParams(/*stepCount=*/4, /*baseTimingIndex=*/3);
    params.latch = true;
    params.gatePercent = 400;
    const LanePattern pattern;

    fixture.updateSettings(params, pattern);
    fixture.engine.startTransport(0.0);
    fixture.setHeldNote(fixture.chordInputChannel, 60, 100);
    fixture.runUntil(0.0);
    REQUIRE(fixture.voices.soundingCount() == 1);

    fixture.output.clear();
    params.latch = false;
    fixture.updateSettings(params, pattern);  // latch on->off: immediate release + clear latch memory

    CHECK(countEventsOfKind(fixture.output, MidiEventKind::noteOff) == 1);
    CHECK(fixture.voices.soundingCount() == 0);
    CHECK_FALSE(fixture.engine.status().hasLatchedNote);
}

TEST_CASE("LaneEngine: after un-mute, latch memory is not restored: only a currently held note plays",
          "[M14][REQ-4.6-6]") {
    LaneFixture fixture;
    auto params = baseTimingParams(/*stepCount=*/4, /*baseTimingIndex=*/3);
    params.latch = true;
    const LanePattern pattern;

    fixture.updateSettings(params, pattern);
    fixture.engine.startTransport(0.0);
    fixture.setHeldNote(fixture.chordInputChannel, 60, 100);
    fixture.runUntil(0.0);
    fixture.releaseHeldNote(fixture.chordInputChannel, 60);  // latch keeps it alive

    params.mute = true;
    fixture.updateSettings(params, pattern);  // mute clears latch memory
    params.mute = false;
    fixture.updateSettings(params, pattern);  // un-mute: no held note -> stays silent

    fixture.output.clear();
    fixture.runUntil(1.0);
    CHECK(fixture.output.size() == 0);
    CHECK_FALSE(fixture.engine.status().hasLatchedNote);
}

TEST_CASE("LaneEngine: release of the source stops the lane immediately, not at the next step start",
          "[M14][REQ-4.2-7][REQ-4.4-2][REQ-9.2-1]") {
    LaneFixture fixture;
    auto params = baseTimingParams(/*stepCount=*/4, /*baseTimingIndex=*/3);
    params.gatePercent = 400;  // the note would otherwise still be sounding well into step 1
    const LanePattern pattern;

    fixture.updateSettings(params, pattern);
    fixture.engine.startTransport(0.0);
    fixture.setHeldNote(fixture.chordInputChannel, 60, 100);
    fixture.runUntil(0.0);
    REQUIRE(fixture.voices.soundingCount() == 1);

    fixture.output.clear();
    const SampleOffset releaseOffset = 123;
    fixture.releaseHeldNote(fixture.chordInputChannel, 60, releaseOffset);

    REQUIRE(countEventsOfKind(fixture.output, MidiEventKind::noteOff) == 1);
    CHECK(fixture.output.events()[0].sampleOffset == releaseOffset);
    CHECK(fixture.voices.soundingCount() == 0);
}

TEST_CASE("LaneEngine: chord re-sorting (pitch change) does not stop the lane and is adopted at the "
          "next step start, leaving the running note untouched",
          "[M14][REQ-4.2-5][REQ-4.2-6]") {
    LaneFixture fixture;
    auto params = baseTimingParams(/*stepCount=*/4, /*baseTimingIndex=*/3 /* 1 beat / step */);
    params.followIndex = 1;
    params.gatePercent = 300;  // note from step 0 is still sounding when step 1 triggers
    const LanePattern pattern;

    fixture.updateSettings(params, pattern);
    fixture.engine.startTransport(0.0);
    fixture.setHeldNote(fixture.chordInputChannel, 60, 100);
    fixture.setHeldNote(fixture.chordInputChannel, 64, 100);
    fixture.setHeldNote(fixture.chordInputChannel, 67, 100);

    fixture.runUntil(0.0);  // step 0: sorted {60,64,67}, index 1 -> 64
    REQUIRE(countEventsOfKindForPitch(fixture.output, MidiEventKind::noteOn, 64) == 1);

    fixture.output.clear();
    fixture.releaseHeldNote(fixture.chordInputChannel, 64);  // chord still has {60,67}: a re-sort, not a release

    CHECK(countEventsOfKind(fixture.output, MidiEventKind::noteOff) == 0);  // pitch 64's note is left running
    CHECK(fixture.voices.soundingCount() == 1);

    fixture.runUntil(1.0);  // step 1: sorted {60,67}, index 1 -> 67
    CHECK(countEventsOfKindForPitch(fixture.output, MidiEventKind::noteOn, 67) == 1);
    CHECK(countEventsOfKindForPitch(fixture.output, MidiEventKind::noteOff, 64) == 0);
}

TEST_CASE("LaneEngine: swing percent is read while scheduling the next step at the preceding step start, "
          "not re-read when that step later triggers",
          "[M14][REQ-5.1-4][REQ-11-5]") {
    LaneFixture fixture;
    auto params = baseTimingParams(/*stepCount=*/4, /*baseTimingIndex=*/3 /* 1/4, 1 beat/step */);
    params.swingPercent = 0;
    const LanePattern pattern;
    const auto referenceLayout = makeUniformGridLayout(baseTimingNoteLength(params.baseTimingIndex, params.subdivision),
                                                        params.stepCount);

    fixture.updateSettings(params, pattern);
    fixture.engine.startTransport(0.0);
    fixture.setHeldNote(fixture.chordInputChannel, 60, 100);

    fixture.runUntil(0.0);  // step 0 triggers; schedules step 1 (odd index) with swing = 0 -> unswung
    CHECK(fixture.engine.nextTimelineBeat() == Catch::Approx(1.0));

    params.swingPercent = 50;
    fixture.updateSettings(params, pattern);  // too late to affect step 1's already-scheduled start
    CHECK(fixture.engine.nextTimelineBeat() == Catch::Approx(1.0));

    fixture.runUntil(1.0);  // step 1 triggers; schedules step 2 (even index -> never swung)
    params.swingPercent = 80;
    fixture.updateSettings(params, pattern);  // still too late for step 2 (already scheduled)

    fixture.runUntil(2.0);  // step 2 triggers; schedules step 3 (odd index) reading swing = 80 "now"
    const Beats expectedAdvance =
        swingAdvanceBeats(referenceLayout.stepLengthTicks[3], 3, 80, referenceLayout.swingApplies);
    CHECK(fixture.engine.nextTimelineBeat() == Catch::Approx(3.0 - expectedAdvance));

    params.swingPercent = 0;
    fixture.updateSettings(params, pattern);  // changing swing again does not move the already-scheduled step 3
    CHECK(fixture.engine.nextTimelineBeat() == Catch::Approx(3.0 - expectedAdvance));
}

TEST_CASE("LaneEngine: a step count decrease re-anchors the phase so the current step index is kept "
          "modulo the new count, with no step skipped or repeated",
          "[M14][REQ-5.4-5]") {
    LaneFixture fixture;
    auto params = baseTimingParams(/*stepCount=*/16, /*baseTimingIndex=*/1 /* 1/16, 0.25 beat/step */);
    const LanePattern pattern;

    fixture.updateSettings(params, pattern);
    fixture.engine.startTransport(0.0);
    fixture.setHeldNote(fixture.chordInputChannel, 60, 100);

    fixture.runUntil(8 * 0.25);  // drive the lane up to and including step 8 (0-based)
    REQUIRE(fixture.engine.status().currentStepIndex == 8);

    params.stepCount = 7;
    fixture.updateSettings(params, pattern);  // pending: committed at the next step start (step index 9)

    fixture.runUntil(9 * 0.25);  // the old-layout step-9 start is where the re-anchoring happens
    CHECK(fixture.engine.status().activeStepCount == 7);
    CHECK(fixture.engine.status().currentStepIndex == 9 % 7);  // = 2, per REQ-5.4-5

    const auto layout7 = makeUniformGridLayout(baseTimingNoteLength(1, Subdivision::straight), 7);
    const Beats stepLengthBeats = ticksToBeats(layout7.stepLengthTicks[0]);
    const Beats reanchoredStart = 9 * 0.25;

    fixture.runUntil(reanchoredStart + stepLengthBeats);
    CHECK(fixture.engine.status().currentStepIndex == 3);
    fixture.runUntil(reanchoredStart + 2 * stepLengthBeats);
    CHECK(fixture.engine.status().currentStepIndex == 4);
}

TEST_CASE("LaneEngine: a step count change guarantees Note-Offs for notes already sounding",
          "[M14][REQ-5.4-6][REQ-9.3-2]") {
    LaneFixture fixture;
    auto params = baseTimingParams(/*stepCount=*/8, /*baseTimingIndex=*/1);
    params.followIndex = 1;
    params.gatePercent = 200;  // notes overlap into the following step(s)
    const LanePattern pattern;

    fixture.updateSettings(params, pattern);
    fixture.engine.startTransport(0.0);
    // Three held notes so a later re-sort (REQ-4.2-5) picks a different pitch at step 1, instead
    // of retriggering (sec. 9.1) the still-scheduled step-0 voice on the same (channel, pitch).
    fixture.setHeldNote(fixture.chordInputChannel, 60, 100);
    fixture.setHeldNote(fixture.chordInputChannel, 64, 100);
    fixture.setHeldNote(fixture.chordInputChannel, 67, 100);
    fixture.runUntil(0.0);  // step 0: sorted {60,64,67}, index 1 -> 64
    REQUIRE(fixture.voices.soundingCount() == 1);

    fixture.releaseHeldNote(fixture.chordInputChannel, 64);  // chord still has {60,67}: a re-sort

    // The scheduled Note-Off must still fire at its beat even after the grid underneath it changes.
    params.stepCount = 4;
    fixture.updateSettings(params, pattern);
    fixture.runUntil(0.25);  // next step start: layout commits, re-anchors; step 1 -> pitch 67

    const auto scheduledEnd = fixture.voices.nextScheduledEnd();
    REQUIRE(scheduledEnd.has_value());
    CHECK(scheduledEnd->beat == Catch::Approx(0.5));  // unaffected: 0.25-beat step * 200 % gate from step 0
}

TEST_CASE("LaneEngine: changes to input mode / follow index take effect at the next step start, "
          "leaving the currently sounding note untouched",
          "[M14][REQ-11-3]") {
    LaneFixture fixture;
    auto params = baseTimingParams(/*stepCount=*/4, /*baseTimingIndex=*/3);
    params.followIndex = 0;
    const LanePattern pattern;

    fixture.updateSettings(params, pattern);
    fixture.engine.startTransport(0.0);
    fixture.setHeldNote(fixture.chordInputChannel, 60, 100);
    fixture.setHeldNote(fixture.chordInputChannel, 64, 100);

    fixture.runUntil(0.0);  // step 0: index 0 -> 60
    REQUIRE(countEventsOfKindForPitch(fixture.output, MidiEventKind::noteOn, 60) == 1);

    params.followIndex = 1;
    fixture.updateSettings(params, pattern);  // pending: must not affect the already-triggered step 0

    fixture.output.clear();
    fixture.runUntil(1.0);  // step 1: now index 1 -> 64
    CHECK(countEventsOfKindForPitch(fixture.output, MidiEventKind::noteOn, 64) == 1);
    CHECK(countEventsOfKindForPitch(fixture.output, MidiEventKind::noteOn, 60) == 0);
}

TEST_CASE("LaneEngine: discard revision masking (D-32) marks discarded tail slots and is cleared once "
          "the pattern document acknowledges them",
          "[M14][REQ-5.4-6]") {
    LaneFixture fixture;
    LanePattern pattern;
    fixture.engine.onPatternAdopted(pattern, /*documentRevision=*/5);  // sets the held document revision

    auto params = baseTimingParams(/*stepCount=*/8, /*baseTimingIndex=*/1);
    fixture.updateSettings(params, pattern);  // first commit (empty -> 8): not a decrease, no discard
    CHECK(fixture.engine.discardRevision() == 0);

    fixture.engine.startTransport(0.0);
    fixture.setHeldNote(fixture.chordInputChannel, 60, 100);
    fixture.runUntil(0.0);

    params.stepCount = 4;  // a real decrease while running: committed (and discarded) at the next step start
    fixture.updateSettings(params, pattern);
    fixture.runUntil(0.25);

    REQUIRE(fixture.engine.discardRevision() == 1);
    const auto& mask = fixture.engine.discardMask();
    for (int slot = 0; slot < 4; ++slot) {
        CHECK(mask[static_cast<std::size_t>(slot)] == 0);
    }
    for (int slot = 4; slot < lanes::maximumStepCount; ++slot) {
        CHECK(mask[static_cast<std::size_t>(slot)] == 5);
    }

    pattern.acknowledgedDiscardRevision = fixture.engine.discardRevision();
    fixture.engine.onPatternAdopted(pattern, /*documentRevision=*/6);  // document caught up: mask cleared

    const auto& clearedMask = fixture.engine.discardMask();
    for (std::size_t slot = 0; slot < clearedMask.size(); ++slot) {
        CHECK(clearedMask[slot] == 0);
    }
    CHECK(fixture.engine.discardRevision() == 1);  // the counter itself is monotonic, never reset
}

TEST_CASE("LaneEngine: step data is one array shared across timing sources", "[M14][REQ-5.2-1]") {
    LaneFixture fixture;
    LanePattern pattern;
    pattern.steps.setValue(0, VelocityMode::byStep, 10);
    pattern.steps.setValue(1, VelocityMode::byStep, 20);
    pattern.steps.setValue(2, VelocityMode::byStep, 30);

    auto params = baseTimingParams(/*stepCount=*/3, /*baseTimingIndex=*/1);
    params.velocityMode = VelocityMode::byStep;

    fixture.updateSettings(params, pattern);
    fixture.engine.startTransport(0.0);
    fixture.setHeldNote(fixture.chordInputChannel, 60, 100);

    fixture.runUntil(0.0);
    fixture.runUntil(0.25);
    fixture.runUntil(0.5);
    // Same pitch triggered every step at gate 100%: consecutive Note-Ons retrigger (sec. 9.1),
    // interleaving Note-Offs, so only the Note-Ons (in order) are relevant here.
    const auto events = noteOnEventsInOrder(fixture.output);
    REQUIRE(events.size() >= 3);
    CHECK(events[0].data2 == 10);
    CHECK(events[1].data2 == 20);
    CHECK(events[2].data2 == 30);

    // Switch to a text grid with the same step count: the SAME pattern.steps array is read.
    fixture.output.clear();
    fixture.engine.reset();
    params.timingSource = TimingSource::textGrid;
    const auto grid = *parseTextGrid("1/8, 1/8, 1/8").grid;
    pattern.appliedTextGrid = grid;

    fixture.updateSettings(params, pattern);
    fixture.engine.startTransport(0.0);
    fixture.setHeldNote(fixture.chordInputChannel, 60, 100);
    fixture.runUntil(0.0);
    fixture.runUntil(ticksToBeats(noteLengthTicks(NoteLength{8, Subdivision::straight})));
    fixture.runUntil(2 * ticksToBeats(noteLengthTicks(NoteLength{8, Subdivision::straight})));

    const auto textGridEvents = noteOnEventsInOrder(fixture.output);
    REQUIRE(textGridEvents.size() >= 3);
    CHECK(textGridEvents[0].data2 == 10);
    CHECK(textGridEvents[1].data2 == 20);
    CHECK(textGridEvents[2].data2 == 30);
}

TEST_CASE("LaneEngine: an empty text grid leaves the lane silent with no timeline events",
          "[M14][REQ-5.2-1]") {
    LaneFixture fixture;
    LaneParameters params;
    params.timingSource = TimingSource::textGrid;  // appliedTextGrid stays default-empty
    const LanePattern pattern;

    fixture.updateSettings(params, pattern);
    fixture.engine.startTransport(0.0);
    fixture.setHeldNote(fixture.chordInputChannel, 60, 100);

    CHECK(fixture.engine.nextTimelineBeat() == std::numeric_limits<Beats>::infinity());
    CHECK(fixture.engine.status().activeStepCount == 0);

    fixture.runUntil(100.0);  // the infinite nextTimelineBeat() means this loop body never runs
    CHECK(fixture.output.size() == 0);
}

TEST_CASE("LaneEngine: stop releases lane voices, clears latch memory, and stops the clock "
          "without re-anchoring",
          "[M14][REQ-4.6-4][REQ-10-6][REQ-9.3-7]") {
    LaneFixture fixture;
    auto params = baseTimingParams(/*stepCount=*/8, /*baseTimingIndex=*/1);
    params.latch = true;
    params.gatePercent = 400;
    const LanePattern pattern;

    fixture.updateSettings(params, pattern);
    fixture.engine.startTransport(0.0);
    fixture.setHeldNote(fixture.chordInputChannel, 60, 100);
    fixture.runUntil(0.0);
    REQUIRE(fixture.voices.soundingCount() == 1);
    REQUIRE(fixture.engine.status().hasLatchedNote);

    // A pending step count decrease, committed by stop() without re-anchoring.
    params.stepCount = 4;
    fixture.updateSettings(params, pattern);

    fixture.output.clear();
    auto ctx = fixture.context();
    fixture.engine.stop(ctx);

    CHECK(countEventsOfKind(fixture.output, MidiEventKind::noteOff) >= 1);
    CHECK_FALSE(fixture.engine.status().isRunning);
    CHECK_FALSE(fixture.engine.status().hasLatchedNote);
    CHECK(fixture.engine.status().activeStepCount == 4);  // pending change committed immediately
}

TEST_CASE("LaneEngine: resynchronize derives the next step from the playhead per D-13 "
          "(first effective start at or after the playhead)",
          "[M14][REQ-10-6][REQ-21-4]") {
    LaneFixture fixture;
    auto params = baseTimingParams(/*stepCount=*/4, /*baseTimingIndex=*/3 /* 1 beat/step */);
    const LanePattern pattern;

    fixture.updateSettings(params, pattern);
    fixture.engine.anchorTransportStart(0.0);  // phase offset 0, loop/step 0, NOT running
    CHECK_FALSE(fixture.engine.status().isRunning);

    auto ctx = fixture.context();
    fixture.engine.resynchronize(/*playheadBeat=*/2.3, /*offset=*/0, /*releaseVoices=*/false, ctx);

    CHECK(fixture.engine.status().isRunning);
    CHECK(fixture.engine.nextTimelineBeat() == Catch::Approx(3.0));  // step 2 starts at 2.0 (< 2.3): skipped
}

TEST_CASE("LaneEngine: resynchronize releases lane voices at the given offset when requested",
          "[M14][REQ-10-6]") {
    LaneFixture fixture;
    auto params = baseTimingParams(/*stepCount=*/4, /*baseTimingIndex=*/3);
    params.gatePercent = 400;
    const LanePattern pattern;

    fixture.updateSettings(params, pattern);
    fixture.engine.startTransport(0.0);
    fixture.setHeldNote(fixture.chordInputChannel, 60, 100);
    fixture.runUntil(0.0);
    REQUIRE(fixture.voices.soundingCount() == 1);

    fixture.output.clear();
    auto ctx = fixture.context();
    const SampleOffset discontinuityOffset = 77;
    fixture.engine.resynchronize(/*playheadBeat=*/5.0, discontinuityOffset, /*releaseVoices=*/true, ctx);

    REQUIRE(countEventsOfKind(fixture.output, MidiEventKind::noteOff) == 1);
    CHECK(fixture.output.events()[0].sampleOffset == discontinuityOffset);
    CHECK(fixture.voices.soundingCount() == 0);
    CHECK(fixture.engine.status().isRunning);
}
