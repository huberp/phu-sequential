#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "arpeggiator/BlockClock.h"
#include "arpeggiator/LanePosition.h"
#include "sequencing/StepTiming.h"
#include "sequencing/NoteValues.h"
#include "sequencing/MusicalTime.h"
#include "sequencing/SampleConversion.h"
#include "sequencing/Constants.h"

// M13 Block clock and lane position: beat<->sample mapping and step-grid position
// derivation (sec. 5.4, 10, 16.2, #10).

using namespace phu::arpeggiator;
using namespace phu::sequencing;
using namespace phu::sequencing::constants;

namespace {

// 4 steps of 1/16 straight = 240 ticks (0.25 beat) each; loop length 1 beat.
GridLayout makeFourStepLayout() {
    return makeUniformGridLayout(NoteLength{16, Subdivision::straight}, 4);
}

}  // namespace

TEST_CASE("LanePosition: derivePosition at the phase offset is loop 0, step 0, at step start",
          "[M13][REQ-10-9]") {
    const auto layout = makeFourStepLayout();
    const auto position = derivePosition(/*playheadBeat=*/2.0, /*phaseOffsetBeat=*/2.0, layout);
    CHECK(position.loopIndex == 0);
    CHECK(position.stepIndex == 0);
    CHECK(position.isAtStepStart);
}

TEST_CASE("LanePosition: derivePosition finds the step containing mid-step playheads",
          "[M13][REQ-10-9]") {
    const auto layout = makeFourStepLayout();
    const auto position = derivePosition(/*playheadBeat=*/0.6, /*phaseOffsetBeat=*/0.0, layout);
    CHECK(position.loopIndex == 0);
    CHECK(position.stepIndex == 2);  // step 2 covers [0.5, 0.75)
    CHECK_FALSE(position.isAtStepStart);
}

TEST_CASE("LanePosition: derivePosition reports isAtStepStart exactly on a step boundary",
          "[M13][REQ-10-9]") {
    const auto layout = makeFourStepLayout();
    const auto position = derivePosition(/*playheadBeat=*/0.5, /*phaseOffsetBeat=*/0.0, layout);
    CHECK(position.stepIndex == 2);
    CHECK(position.isAtStepStart);
}

TEST_CASE("LanePosition: derivePosition tolerates beatEpsilon jitter around a step boundary",
          "[M13][REQ-10-9]") {
    const auto layout = makeFourStepLayout();
    const auto justBefore = derivePosition(0.5 - timing::beatEpsilon / 2.0, 0.0, layout);
    const auto justAfter = derivePosition(0.5 + timing::beatEpsilon / 2.0, 0.0, layout);
    CHECK(justBefore.isAtStepStart);
    CHECK(justAfter.isAtStepStart);
}

TEST_CASE("LanePosition: derivePosition wraps into the next loop", "[M13][REQ-10-9]") {
    const auto layout = makeFourStepLayout();
    const auto position = derivePosition(/*playheadBeat=*/1.5, /*phaseOffsetBeat=*/0.0, layout);
    CHECK(position.loopIndex == 1);
    CHECK(position.stepIndex == 2);
}

TEST_CASE("LanePosition: derivePosition handles playheads before the phase offset (negative loop index)",
          "[M13][REQ-10-9]") {
    const auto layout = makeFourStepLayout();
    // playhead - phaseOffset = -0.5 beats; mod 1.0 beat loop length = 0.5 beats into loop -1.
    const auto position = derivePosition(/*playheadBeat=*/0.5, /*phaseOffsetBeat=*/1.0, layout);
    CHECK(position.loopIndex == -1);
    CHECK(position.stepIndex == 2);
}

TEST_CASE("LanePosition: derivePosition at the loop index example from REQ-10-9 (0.5/1.0/4 -> 0.5)",
          "[M13][REQ-10-9]") {
    const auto layout = makeFourStepLayout();
    const auto position = derivePosition(/*playheadBeat=*/5.5, /*phaseOffsetBeat=*/1.0, layout);
    CHECK(position.loopIndex == 4);
    CHECK(position.stepIndex == 2);  // position within loop == 0.5 beats -> step 2
}

TEST_CASE("LanePosition: stepStartBeat computes the absolute beat of a step in loop 0",
          "[M13][REQ-5.4]") {
    const auto layout = makeFourStepLayout();
    CHECK(stepStartBeat(/*phaseOffsetBeat=*/2.0, /*loopIndex=*/0, /*stepIndex=*/2, layout) ==
          Catch::Approx(2.5));
}

TEST_CASE("LanePosition: stepStartBeat accounts for the loop index", "[M13][REQ-5.4]") {
    const auto layout = makeFourStepLayout();
    CHECK(stepStartBeat(/*phaseOffsetBeat=*/0.0, /*loopIndex=*/3, /*stepIndex=*/0, layout) ==
          Catch::Approx(3.0));
    CHECK(stepStartBeat(/*phaseOffsetBeat=*/0.0, /*loopIndex=*/-1, /*stepIndex=*/2, layout) ==
          Catch::Approx(-0.5));
}

TEST_CASE("LanePosition: stepStartBeat is consistent with derivePosition (round-trip)",
          "[M13][REQ-5.4]") {
    const auto layout = makeFourStepLayout();
    const Beats phaseOffset = 0.3;
    const Beats playhead = 2.85;
    const auto position = derivePosition(playhead, phaseOffset, layout);
    const auto recoveredStart = stepStartBeat(phaseOffset, position.loopIndex, position.stepIndex, layout);
    CHECK(recoveredStart <= playhead + timing::beatEpsilon);
    CHECK(playhead - recoveredStart < ticksToBeats(layout.stepLengthTicks[static_cast<std::size_t>(position.stepIndex)]));
}

TEST_CASE("LanePosition: reanchorPhaseOffset places the given step of the new layout at the commit beat",
          "[M13][REQ-5.4-5]") {
    const auto oldLayout = makeUniformGridLayout(NoteLength{16, Subdivision::straight}, 16);
    const auto newLayout = makeUniformGridLayout(NoteLength{16, Subdivision::straight}, 7);
    const Beats commitBeat = 10.0;
    const int newStepIndex = 9 % 7;  // REQ-5.4-5: current index 9 kept modulo the new step count (= 2)
    REQUIRE(newStepIndex == 2);

    const Beats phaseOffset = reanchorPhaseOffset(commitBeat, newStepIndex, newLayout);
    const auto recomputedStart = stepStartBeat(phaseOffset, /*loopIndex=*/0, newStepIndex, newLayout);
    CHECK(recomputedStart == Catch::Approx(commitBeat));
    (void)oldLayout;
}

TEST_CASE("LanePosition: reanchorPhaseOffset for step 0 sets the phase offset to the commit beat",
          "[M13][REQ-5.4-5]") {
    const auto layout = makeFourStepLayout();
    const Beats phaseOffset = reanchorPhaseOffset(/*commitBeat=*/5.0, /*newStepIndex=*/0, layout);
    CHECK(phaseOffset == Catch::Approx(5.0));
}

TEST_CASE("BlockClock: toSampleOffset maps the block start beat to sample 0", "[M13][REQ-10-2]") {
    BlockClock clock;
    clock.blockStartBeat = 2.0;
    clock.samplesPerBeat = 1000.0;
    clock.numSamples = 512;
    clock.windowStartBeat = clock.blockStartBeat;
    clock.windowEndBeat = clock.blockStartBeat + clock.numSamples / clock.samplesPerBeat;

    CHECK(clock.toSampleOffset(2.0) == 0);
    CHECK(clock.toSampleOffset(2.1) == static_cast<SampleOffset>(100));
}

TEST_CASE("BlockClock: toSampleOffset clamps to 0..numSamples-1", "[M13]") {
    BlockClock clock;
    clock.blockStartBeat = 0.0;
    clock.samplesPerBeat = 1000.0;
    clock.numSamples = 100;
    clock.windowStartBeat = 0.0;
    clock.windowEndBeat = 0.1;

    CHECK(clock.toSampleOffset(-1.0) == 0);
    CHECK(clock.toSampleOffset(10.0) == 99);
}

TEST_CASE("BlockClock: toBeat is the inverse of toSampleOffset at exact sample boundaries",
          "[M13]") {
    BlockClock clock;
    clock.blockStartBeat = 1.0;
    clock.samplesPerBeat = 500.0;
    clock.numSamples = 1000;
    clock.windowStartBeat = clock.blockStartBeat;
    clock.windowEndBeat = clock.blockStartBeat + clock.numSamples / clock.samplesPerBeat;

    CHECK(clock.toBeat(0) == Catch::Approx(1.0));
    CHECK(clock.toBeat(500) == Catch::Approx(2.0));
}

TEST_CASE("BlockClock: contains is true only within windowStartBeat inclusive and windowEndBeat exclusive",
          "[M13][REQ-10-2]") {
    BlockClock clock;
    clock.blockStartBeat = 0.0;
    clock.windowStartBeat = 0.0;
    clock.windowEndBeat = 1.0;
    clock.samplesPerBeat = 1000.0;
    clock.numSamples = 1000;

    CHECK(clock.contains(0.0));
    CHECK(clock.contains(0.999));
    CHECK_FALSE(clock.contains(1.0));
    CHECK_FALSE(clock.contains(-0.001));
}
