#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "arpeggiator/DisplayScale.h"
#include "sequencing/Constants.h"
#include <array>

// M23c Display scale: the shared px/beat helper for §14.
using namespace phu::sequencing;
using namespace phu::arpeggiator;

namespace {
std::array<GridLayout, constants::lanes::laneCount> makeLanes() {
    return std::array<GridLayout, constants::lanes::laneCount>{};  // all empty by default
}
} // namespace

TEST_CASE("DisplayScale: 12x1/8 lane is exactly four 1/8-step widths wider than an 8x1/8 lane, "
          "and a 7x1/8 lane is one 1/8-step width narrower than the 8x1/8 lane (§14)",
          "[M23c][display-scale][REQ-14-shared-scale]") {
    auto lanes = makeLanes();
    lanes[0] = makeUniformGridLayout(NoteLength{8, Subdivision::straight}, 12);
    lanes[1] = makeUniformGridLayout(NoteLength{8, Subdivision::straight}, 8);
    lanes[2] = makeUniformGridLayout(NoteLength{8, Subdivision::straight}, 7);

    const auto scale = computeDisplayScale(lanes, 960.0);

    const double oneStepWidthPixels = ticksToBeats(lanes[1].stepLengthTicks[0]) * scale.pixelsPerBeat;
    REQUIRE(oneStepWidthPixels > 0.0);

    CHECK(scale.lanes[0].loopWidthPixels - scale.lanes[1].loopWidthPixels ==
          Catch::Approx(4 * oneStepWidthPixels));
    CHECK(scale.lanes[1].loopWidthPixels - scale.lanes[2].loopWidthPixels ==
          Catch::Approx(1 * oneStepWidthPixels));

    // The scale is driven by the longest loop (the 12-step lane).
    CHECK(scale.contentWidthPixels == Catch::Approx(scale.lanes[0].loopWidthPixels));
}

TEST_CASE("DisplayScale: pixelsPerBeat is availableWidth / longestLoopBeats when unclamped",
          "[M23c][display-scale]") {
    auto lanes = makeLanes();
    lanes[0] = makeUniformGridLayout(NoteLength{8, Subdivision::straight}, 12);  // 6 beats

    const auto scale = computeDisplayScale(lanes, 960.0);
    CHECK(scale.pixelsPerBeat == Catch::Approx(960.0 / 6.0));
    CHECK(scale.lanes[0].stepCount == 12);
    CHECK(scale.lanes[0].stepStartPixels[0] == Catch::Approx(0.0));
    CHECK(scale.lanes[0].stepStartPixels[12] == Catch::Approx(scale.lanes[0].loopWidthPixels));
}

TEST_CASE("DisplayScale: scale is clamped so no step is narrower than minimumStepDisplayWidth (§14)",
          "[M23c][display-scale][REQ-14-min-step-width]") {
    auto lanes = makeLanes();
    lanes[0] = makeUniformGridLayout(NoteLength{16, Subdivision::straight}, 4);  // step = 0.25 beat, loop = 1 beat

    // Tiny available width: naive scale (1 px/beat) would make each step 0.25px wide.
    const auto scale = computeDisplayScale(lanes, 1.0);

    const double expectedClampedScale =
        static_cast<double>(constants::editor::minimumStepDisplayWidth) / 0.25;
    CHECK(scale.pixelsPerBeat == Catch::Approx(expectedClampedScale));
    const double stepWidthPixels = 0.25 * scale.pixelsPerBeat;
    CHECK(stepWidthPixels == Catch::Approx(static_cast<double>(constants::editor::minimumStepDisplayWidth)));
}

TEST_CASE("DisplayScale: all lanes empty yields a zero scale and zero widths", "[M23c][display-scale]") {
    auto lanes = makeLanes();
    const auto scale = computeDisplayScale(lanes, 1000.0);
    CHECK(scale.pixelsPerBeat == 0.0);
    CHECK(scale.contentWidthPixels == 0.0);
    for (const auto& lane : scale.lanes) {
        CHECK(lane.stepCount == 0);
        CHECK(lane.loopWidthPixels == 0.0);
    }
}
