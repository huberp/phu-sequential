#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "sequencing/StepTiming.h"
#include "sequencing/Constants.h"
#include <array>

// M03 Step timing: grid layout, swing, gate, step index lookup (§5.1, §5.2, §5.4, §6).
using namespace phu::sequencing;

TEST_CASE("StepTiming: makeUniformGridLayout computes loop length and step start positions",
          "[M03][step-timing][REQ-5.4-loop-length]") {
    const auto layout = makeUniformGridLayout(NoteLength{16, Subdivision::straight}, 4);
    REQUIRE(layout.stepCount == 4);
    CHECK_FALSE(layout.isEmpty());
    CHECK(layout.swingApplies);
    // 1/16 straight = 240 ticks; loop length = 4 * 240 = 960 ticks = 1 beat.
    CHECK(layout.loopLengthTicks() == 960);
    CHECK(layout.loopLengthBeats() == 1.0);
    CHECK(layout.stepStartTicks[0] == 0);
    CHECK(layout.stepStartTicks[1] == 240);
    CHECK(layout.stepStartTicks[2] == 480);
    CHECK(layout.stepStartTicks[3] == 720);
    CHECK(layout.stepStartTicks[4] == 960);  // [stepCount] = loop length
}

TEST_CASE("StepTiming: makeUniformGridLayout clamps step count to [1, 64]",
          "[M03][step-timing][REQ-5.4-step-count]") {
    CHECK(makeUniformGridLayout(NoteLength{16, Subdivision::straight}, 0).stepCount == 1);
    CHECK(makeUniformGridLayout(NoteLength{16, Subdivision::straight}, -5).stepCount == 1);
    CHECK(makeUniformGridLayout(NoteLength{16, Subdivision::straight}, 100).stepCount == 64);
}

TEST_CASE("StepTiming: 12 steps of 1/8 vs 8 steps of 1/8 differ by exactly 4 step widths (sec. 14)",
          "[M03][step-timing][REQ-14-display-scale]") {
    const auto twelve = makeUniformGridLayout(NoteLength{8, Subdivision::straight}, 12);
    const auto eight = makeUniformGridLayout(NoteLength{8, Subdivision::straight}, 8);
    const auto seven = makeUniformGridLayout(NoteLength{8, Subdivision::straight}, 7);
    const auto oneStepBeats = ticksToBeats(eight.stepLengthTicks[0]);
    CHECK(twelve.loopLengthBeats() - eight.loopLengthBeats() == Catch::Approx(4 * oneStepBeats));
    CHECK(eight.loopLengthBeats() - seven.loopLengthBeats() == Catch::Approx(1 * oneStepBeats));
}

TEST_CASE("StepTiming: makeGridLayout builds a text grid and marks swing as inapplicable",
          "[M03][step-timing][REQ-5.2-text-grid]") {
    const std::array<NoteLength, 3> entries{
        NoteLength{8, Subdivision::triplet}, NoteLength{8, Subdivision::dotted}, NoteLength{4, Subdivision::straight}};
    const auto layout = makeGridLayout(entries);
    REQUIRE(layout.stepCount == 3);
    CHECK_FALSE(layout.swingApplies);
    CHECK(layout.stepLengthTicks[0] == noteLengthTicks(entries[0]));
    CHECK(layout.stepLengthTicks[1] == noteLengthTicks(entries[1]));
    CHECK(layout.stepLengthTicks[2] == noteLengthTicks(entries[2]));
    CHECK(layout.loopLengthTicks() ==
          noteLengthTicks(entries[0]) + noteLengthTicks(entries[1]) + noteLengthTicks(entries[2]));
}

TEST_CASE("StepTiming: makeGridLayout with an empty span is the empty (silent) layout",
          "[M03][step-timing][REQ-5.2-empty-grid]") {
    const auto layout = makeGridLayout(std::span<const NoteLength>{});
    CHECK(layout.isEmpty());
    CHECK(layout.stepCount == 0);
}

TEST_CASE("StepTiming: GridLayout equality compares stepCount, swingApplies and step lengths only",
          "[M03][step-timing]") {
    const auto a = makeUniformGridLayout(NoteLength{16, Subdivision::straight}, 4);
    const auto b = makeUniformGridLayout(NoteLength{16, Subdivision::straight}, 4);
    const auto c = makeUniformGridLayout(NoteLength{16, Subdivision::straight}, 8);
    CHECK(a == b);
    CHECK_FALSE(a == c);
}

TEST_CASE("StepTiming: swingAdvanceBeats is 0 for even 0-based steps and for text grids",
          "[M03][step-timing][REQ-5.1-swing-0]") {
    const TickCount stepLengthTicks = noteLengthTicks(NoteLength{16, Subdivision::straight});  // 240 ticks
    CHECK(swingAdvanceBeats(stepLengthTicks, 0, 50, true) == 0.0);
    CHECK(swingAdvanceBeats(stepLengthTicks, 2, 100, true) == 0.0);
    CHECK(swingAdvanceBeats(stepLengthTicks, 1, 50, false) == 0.0);  // text grid: swing never applies
}

TEST_CASE("StepTiming: swingAdvanceBeats at 50% matches the sec. 3.3 worked example",
          "[M03][step-timing][REQ-5.1-swing-50]") {
    const auto layout = makeUniformGridLayout(NoteLength{16, Subdivision::straight}, 4);  // step length 0.25 beat
    const int swingPercent = 50;
    const auto advance = swingAdvanceBeats(layout.stepLengthTicks[1], 1, swingPercent, layout.swingApplies);
    CHECK(advance == Catch::Approx(0.0625));
    const auto g1 = ticksToBeats(layout.stepStartTicks[1]);
    const auto g3 = ticksToBeats(layout.stepStartTicks[3]);
    CHECK(g1 - advance == Catch::Approx(0.1875));
    CHECK(g3 - swingAdvanceBeats(layout.stepLengthTicks[3], 3, swingPercent, layout.swingApplies) ==
          Catch::Approx(0.6875));
}

TEST_CASE("StepTiming: swingAdvanceBeats at 100% shifts by half a step (sec. 5.1)",
          "[M03][step-timing][REQ-5.1-swing-100]") {
    const auto layout = makeUniformGridLayout(NoteLength{16, Subdivision::straight}, 4);
    const auto advance = swingAdvanceBeats(layout.stepLengthTicks[1], 1, 100, layout.swingApplies);
    CHECK(advance == Catch::Approx(0.125));
    const auto g1 = ticksToBeats(layout.stepStartTicks[1]);
    CHECK(g1 - advance == Catch::Approx(0.125));
}

TEST_CASE("StepTiming: swingAdvanceBeats clamps swing percent to [0, 100]",
          "[M03][step-timing]") {
    const TickCount stepLengthTicks = noteLengthTicks(NoteLength{16, Subdivision::straight});
    CHECK(swingAdvanceBeats(stepLengthTicks, 1, 150, true) ==
          swingAdvanceBeats(stepLengthTicks, 1, 100, true));
    CHECK(swingAdvanceBeats(stepLengthTicks, 1, -20, true) == 0.0);
}

TEST_CASE("StepTiming: gatedLengthBeats scales the step length and clamps gate percent",
          "[M03][step-timing][REQ-6-gate]") {
    const TickCount stepLengthTicks = noteLengthTicks(NoteLength{4, Subdivision::straight});  // 1 beat
    CHECK(gatedLengthBeats(stepLengthTicks, 100) == Catch::Approx(1.0));
    CHECK(gatedLengthBeats(stepLengthTicks, 50) == Catch::Approx(0.5));
    CHECK(gatedLengthBeats(stepLengthTicks, 200) == Catch::Approx(2.0));
    CHECK(gatedLengthBeats(stepLengthTicks, 0) == Catch::Approx(0.01));    // clamped up to minimum 1%
    CHECK(gatedLengthBeats(stepLengthTicks, 300) == Catch::Approx(2.0));   // clamped down to maximum 200%
}

TEST_CASE("StepTiming: stepIndexAtTick finds the step containing a tick position",
          "[M03][step-timing]") {
    const auto layout = makeUniformGridLayout(NoteLength{16, Subdivision::straight}, 4);  // 240 ticks/step
    CHECK(stepIndexAtTick(layout, 0) == 0);
    CHECK(stepIndexAtTick(layout, 239) == 0);
    CHECK(stepIndexAtTick(layout, 240) == 1);
    CHECK(stepIndexAtTick(layout, 719) == 2);
    CHECK(stepIndexAtTick(layout, 720) == 3);
    CHECK(stepIndexAtTick(layout, 959) == 3);
}

TEST_CASE("StepTiming: stepIndexAtTick on an empty layout returns 0", "[M03][step-timing]") {
    const auto layout = makeGridLayout(std::span<const NoteLength>{});
    CHECK(stepIndexAtTick(layout, 0) == 0);
}
