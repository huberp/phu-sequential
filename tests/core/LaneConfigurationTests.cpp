#include <catch2/catch_test_macros.hpp>
#include "arpeggiator/LaneConfiguration.h"
#include "sequencing/StepTiming.h"
#include "sequencing/NoteValues.h"
#include "sequencing/TextGridParser.h"
#include "sequencing/sources/InputModeRegistry.h"
#include "sequencing/velocity/VelocityMode.h"
#include "sequencing/Constants.h"

// M12 Lane configuration: parameter clamping, active grid derivation, effective step
// count, and source selector construction (sec. 4.1-4.6, 5.1-5.4, 12, 20, #10).

using namespace phu::arpeggiator;
using namespace phu::sequencing;
using namespace phu::sequencing::constants;

namespace {

TextGrid makeValidTextGrid(std::string_view text) {
    const auto result = parseTextGrid(text);
    REQUIRE(result.isValid());
    return *result.grid;
}

}  // namespace

TEST_CASE("LaneConfiguration: clampLaneParameters clamps followIndex to 0..15",
          "[M12][REQ-4.2-9]") {
    LaneParameters params;
    params.followIndex = -1;
    CHECK(clampLaneParameters(params).followIndex == 0);

    params.followIndex = 16;
    CHECK(clampLaneParameters(params).followIndex == lanes::maximumFollowIndex);
}

TEST_CASE("LaneConfiguration: clampLaneParameters clamps followChannel and outputChannel to 1..16",
          "[M12]") {
    LaneParameters params;
    params.followChannel = 0;
    params.outputChannel = 17;
    const auto clamped = clampLaneParameters(params);
    CHECK(clamped.followChannel == midi::firstChannel);
    CHECK(clamped.outputChannel == midi::lastChannel);
}

TEST_CASE("LaneConfiguration: clampLaneParameters clamps transpose to -24..24",
          "[M12][REQ-8-3]") {
    LaneParameters params;
    params.transpose = 30;
    CHECK(clampLaneParameters(params).transpose == lanes::maximumTranspose);

    params.transpose = -30;
    CHECK(clampLaneParameters(params).transpose == lanes::minimumTranspose);
}

TEST_CASE("LaneConfiguration: clampLaneParameters clamps gatePercent to 1..200",
          "[M12][REQ-6-3]") {
    LaneParameters params;
    params.gatePercent = 0;
    CHECK(clampLaneParameters(params).gatePercent == lanes::minimumGatePercent);

    params.gatePercent = 500;
    CHECK(clampLaneParameters(params).gatePercent == lanes::maximumGatePercent);
}

TEST_CASE("LaneConfiguration: clampLaneParameters clamps swingPercent to 0..100",
          "[M12][REQ-5.1-3]") {
    LaneParameters params;
    params.swingPercent = 150;
    CHECK(clampLaneParameters(params).swingPercent == lanes::maximumSwingPercent);

    params.swingPercent = -5;
    CHECK(clampLaneParameters(params).swingPercent == lanes::minimumSwingPercent);
}

TEST_CASE("LaneConfiguration: clampLaneParameters clamps stepCount to 1..64",
          "[M12][REQ-5.4-1]") {
    LaneParameters params;
    params.stepCount = 0;
    CHECK(clampLaneParameters(params).stepCount == lanes::minimumStepCount);

    params.stepCount = 65;
    CHECK(clampLaneParameters(params).stepCount == lanes::maximumStepCount);
}

TEST_CASE("LaneConfiguration: clampLaneParameters clamps baseTimingIndex into the registry range",
          "[M12]") {
    LaneParameters params;
    params.baseTimingIndex = -1;
    CHECK(clampLaneParameters(params).baseTimingIndex == 0);

    params.baseTimingIndex = 99;
    CHECK(clampLaneParameters(params).baseTimingIndex ==
          static_cast<int>(baseTimingRegistry().size()) - 1);
}

TEST_CASE("LaneConfiguration: clampLaneParameters leaves already-valid parameters unchanged",
          "[M12]") {
    LaneParameters params;
    params.mute = true;
    params.latch = true;
    params.inputMode = InputMode::channel;
    params.followIndex = 3;
    params.followChannel = 5;
    params.outputChannel = 7;
    params.transpose = 12;
    params.gatePercent = 150;
    params.timingSource = TimingSource::textGrid;
    params.baseTimingIndex = 2;
    params.subdivision = Subdivision::triplet;
    params.swingPercent = 40;
    params.stepCount = 7;
    params.velocityMode = VelocityMode::byStep;

    CHECK(clampLaneParameters(params) == params);
}

TEST_CASE("LaneConfiguration: makeActiveGridLayout uses base timing when timingSource is baseTiming",
          "[M12][REQ-5.1-1]") {
    LaneParameters params;
    params.timingSource = TimingSource::baseTiming;
    params.baseTimingIndex = 1;  // "1/16"
    params.subdivision = Subdivision::straight;
    params.stepCount = 8;

    const TextGrid emptyGrid{};
    const auto layout = makeActiveGridLayout(params, emptyGrid);
    const auto expected =
        makeUniformGridLayout(baseTimingNoteLength(params.baseTimingIndex, params.subdivision), params.stepCount);

    REQUIRE(layout.stepCount == expected.stepCount);
    CHECK(layout.swingApplies == expected.swingApplies);
    CHECK(layout.loopLengthTicks() == expected.loopLengthTicks());
}

TEST_CASE("LaneConfiguration: makeActiveGridLayout uses the applied text grid when timingSource is textGrid",
          "[M12][REQ-5.2-1]") {
    LaneParameters params;
    params.timingSource = TimingSource::textGrid;
    params.stepCount = 16;  // irrelevant while textGrid is active

    const auto grid = makeValidTextGrid("1/8t, 1/8., 1/4");
    const auto layout = makeActiveGridLayout(params, grid);
    const auto expected = makeGridLayout(grid.activeEntries());

    REQUIRE(layout.stepCount == 3);
    CHECK_FALSE(layout.swingApplies);
    CHECK(layout.loopLengthTicks() == expected.loopLengthTicks());
}

TEST_CASE("LaneConfiguration: makeActiveGridLayout is empty (silent) when the applied text grid is empty",
          "[M12][REQ-5.2-1]") {
    LaneParameters params;
    params.timingSource = TimingSource::textGrid;

    const TextGrid emptyGrid{};
    const auto layout = makeActiveGridLayout(params, emptyGrid);
    CHECK(layout.isEmpty());
}

TEST_CASE("LaneConfiguration: makeActiveGridLayout ignores the applied text grid in base-timing mode",
          "[M12][REQ-5.2-1]") {
    LaneParameters params;
    params.timingSource = TimingSource::baseTiming;
    params.baseTimingIndex = 3;  // "1/4"
    params.stepCount = 4;

    const auto grid = makeValidTextGrid("1/8t, 1/8., 1/4");
    const auto layout = makeActiveGridLayout(params, grid);
    CHECK(layout.swingApplies);
    CHECK(layout.stepCount == 4);
}

TEST_CASE("LaneConfiguration: effectiveStepCount matches the active grid's step count for base timing",
          "[M12]") {
    LaneParameters params;
    params.timingSource = TimingSource::baseTiming;
    params.stepCount = 11;

    const TextGrid emptyGrid{};
    CHECK(effectiveStepCount(params, emptyGrid) == 11);
}

TEST_CASE("LaneConfiguration: effectiveStepCount matches the text grid entry count for text grid",
          "[M12]") {
    LaneParameters params;
    params.timingSource = TimingSource::textGrid;

    const auto grid = makeValidTextGrid("1/8, 1/8, 1/8, 1/8, 1/8");
    CHECK(effectiveStepCount(params, grid) == 5);
}

TEST_CASE("LaneConfiguration: effectiveStepCount is 0 for an empty text grid",
          "[M12][REQ-5.2-1]") {
    LaneParameters params;
    params.timingSource = TimingSource::textGrid;

    const TextGrid emptyGrid{};
    CHECK(effectiveStepCount(params, emptyGrid) == 0);
}

TEST_CASE("LaneConfiguration: makeSourceSelector copies chord-follow fields and the global chord channel",
          "[M12][REQ-4.2-1]") {
    LaneParameters params;
    params.inputMode = InputMode::chordFollow;
    params.followIndex = 2;
    params.followChannel = 9;  // irrelevant in chord-follow mode, still copied verbatim

    const auto selector = makeSourceSelector(params, /*chordInputChannel=*/3);
    CHECK(selector.inputMode == InputMode::chordFollow);
    CHECK(selector.followIndex == 2);
    CHECK(selector.followChannel == 9);
    CHECK(selector.chordInputChannel == 3);
}

TEST_CASE("LaneConfiguration: makeSourceSelector copies channel-mode fields",
          "[M12][REQ-4.3-1]") {
    LaneParameters params;
    params.inputMode = InputMode::channel;
    params.followChannel = 6;

    const auto selector = makeSourceSelector(params, /*chordInputChannel=*/1);
    CHECK(selector.inputMode == InputMode::channel);
    CHECK(selector.followChannel == 6);
}

TEST_CASE("LaneConfiguration: timingSourceRegistry has one entry per TimingSource value",
          "[M12]") {
    REQUIRE(timingSourceRegistry.size() == static_cast<std::size_t>(timingSourceCount));
    CHECK(static_cast<std::string>(timingSourceRegistry[static_cast<int>(TimingSource::baseTiming)].persistenceKey) ==
          "baseTiming");
    CHECK(static_cast<std::string>(timingSourceRegistry[static_cast<int>(TimingSource::textGrid)].persistenceKey) ==
          "textGrid");
}

TEST_CASE("LaneConfiguration: LaneParameters defaults match sec. 20 (initial state)",
          "[M12][REQ-20-1][REQ-20-2]") {
    const LaneParameters params;
    CHECK(params.mute == false);
    CHECK(params.latch == false);
    CHECK(params.inputMode == InputMode::chordFollow);
    CHECK(params.followIndex == 0);
    CHECK(params.outputChannel == 1);
    CHECK(params.transpose == 0);
    CHECK(params.gatePercent == 100);
    CHECK(params.timingSource == TimingSource::baseTiming);
    CHECK(params.subdivision == Subdivision::straight);
    CHECK(params.swingPercent == 0);
    CHECK(params.stepCount == 16);
    CHECK(params.velocityMode == VelocityMode::incoming);
}

TEST_CASE("LaneConfiguration: GlobalParameters defaults match sec. 20", "[M12][REQ-20-2]") {
    const GlobalParameters params;
    CHECK(params.thru == false);
    CHECK(params.consume == true);
    CHECK(params.chordInputChannel == 1);
}
