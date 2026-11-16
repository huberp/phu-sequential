#include <catch2/catch_test_macros.hpp>
#include "arpeggiator/ConfigurationBuilder.h"

// M20 core: raw parameter snapshot -> validated SequencerConfiguration (sec. 12, 17, 20, #13).

using namespace phu::arpeggiator;
using namespace phu::sequencing;
using namespace phu::sequencing::constants;

TEST_CASE("ConfigurationBuilder: default snapshot equals sec. 20 defaults",
          "[M20][REQ-12-5][REQ-20-3]") {
    CHECK(buildConfiguration(ParameterSnapshot{}) == defaultSequencerConfiguration());
}

TEST_CASE("ConfigurationBuilder: raw values map to configuration fields",
          "[M20][REQ-12-5]") {
    ParameterSnapshot snapshot;
    snapshot.thru = 1;
    snapshot.consume = 0;
    snapshot.chordInputChannel = 5;
    auto& raw = snapshot.lanes[3];
    raw = {1, 1, 1, 7, 9, 12, -5, 150, 1, 3, 2, 40, 33, 3};

    const auto config = buildConfiguration(snapshot);
    CHECK(config.global.thru);
    CHECK_FALSE(config.global.consume);
    CHECK(config.global.chordInputChannel == 5);

    const auto& lane = config.lanes[3];
    CHECK(lane.mute);
    CHECK(lane.latch);
    CHECK(lane.inputMode == InputMode::channel);
    CHECK(lane.followIndex == 7);
    CHECK(lane.followChannel == 9);
    CHECK(lane.outputChannel == 12);
    CHECK(lane.transpose == -5);
    CHECK(lane.gatePercent == 150);
    CHECK(lane.timingSource == TimingSource::textGrid);
    CHECK(lane.baseTimingIndex == 3);
    CHECK(lane.subdivision == Subdivision::dotted);
    CHECK(lane.swingPercent == 40);
    CHECK(lane.stepCount == 33);
    CHECK(lane.velocityMode == VelocityMode::relative);
    CHECK(config.lanes[0] == LaneParameters{});
}

TEST_CASE("ConfigurationBuilder: out-of-range values are clamped",
          "[M20][REQ-12-5][REQ-4.2-9]") {
    ParameterSnapshot snapshot;
    snapshot.chordInputChannel = 99;
    snapshot.lanes[0] = {5, -2, 99, 16, 0, 17, 30, 0, 7, 99, -1, 101, 0, -4};
    snapshot.lanes[1] = {0, 0, -3, -1, 99, -4, -30, 999, -1, -9, 77, -5, 65, 40};

    const auto config = buildConfiguration(snapshot);
    CHECK(config.global.chordInputChannel == midi::lastChannel);

    const auto& a = config.lanes[0];
    CHECK(a.inputMode == InputMode::channel);
    CHECK(a.followIndex == lanes::maximumFollowIndex);
    CHECK(a.followChannel == midi::firstChannel);
    CHECK(a.outputChannel == midi::lastChannel);
    CHECK(a.transpose == lanes::maximumTranspose);
    CHECK(a.gatePercent == lanes::minimumGatePercent);
    CHECK(a.timingSource == TimingSource::textGrid);
    CHECK(a.baseTimingIndex == static_cast<int>(baseTimingRegistry().size()) - 1);
    CHECK(a.subdivision == Subdivision::straight);
    CHECK(a.swingPercent == lanes::maximumSwingPercent);
    CHECK(a.stepCount == lanes::minimumStepCount);
    CHECK(a.velocityMode == VelocityMode::incoming);

    const auto& b = config.lanes[1];
    CHECK(b.inputMode == InputMode::chordFollow);
    CHECK(b.followIndex == lanes::minimumFollowIndex);
    CHECK(b.followChannel == midi::lastChannel);
    CHECK(b.outputChannel == midi::firstChannel);
    CHECK(b.transpose == lanes::minimumTranspose);
    CHECK(b.gatePercent == lanes::maximumGatePercent);
    CHECK(b.timingSource == TimingSource::baseTiming);
    CHECK(b.baseTimingIndex == 0);
    CHECK(b.subdivision == Subdivision::dotted);
    CHECK(b.swingPercent == lanes::minimumSwingPercent);
    CHECK(b.stepCount == lanes::maximumStepCount);
    CHECK(b.velocityMode == VelocityMode::relative);
}
