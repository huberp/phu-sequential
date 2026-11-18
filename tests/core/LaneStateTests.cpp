#include <catch2/catch_test_macros.hpp>

#include "arpeggiator/LaneState.h"
#include "sequencing/Constants.h"

// M26 core part: LaneState value semantics (sec. 22.3).

using namespace phu::arpeggiator;
using namespace phu::sequencing;
using namespace phu::sequencing::constants;

TEST_CASE("LaneState: default state matches the sec. 20 lane defaults", "[M26][REQ-22-1]") {
    for (int i = 0; i < lanes::laneCount; ++i) {
        const auto state = defaultLaneState(i);
        CHECK(state.configuration == LaneParameters{});
        CHECK(state.document == PatternDocument{}.lane(i));
        CHECK(state.document.color == defaultLaneColor(i));
    }
}

TEST_CASE("LaneState: copy is deep and operator== compares all properties", "[M26][REQ-22-1]") {
    const auto original = defaultLaneState(0);
    auto copy = original;
    CHECK(copy == original);

    copy.configuration.transpose = 5;
    CHECK(copy != original);
    CHECK(original.configuration.transpose == lanes::defaultTranspose);

    auto other = original;
    other.document.color = LaneColor{1, 2, 3};
    CHECK(other != original);
    other = original;
    other.document.textGridText = "1/8";
    CHECK(other != original);
    other = original;
    other.document.pattern.steps.setEnabled(0, false);
    CHECK(other != original);
    CHECK(original.document.pattern.steps.step(0).enabled);

    copy.configuration.transpose = lanes::defaultTranspose;
    CHECK(copy == original);
}
