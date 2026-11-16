#include <catch2/catch_test_macros.hpp>

#include "arpeggiator/PatternDocument.h"
#include "sequencing/Constants.h"

#include <set>
#include <string>

// M21 core part: PatternDocument (sec. 5.3, 12, 13, 20, #14).

using namespace phu::arpeggiator;
using namespace phu::sequencing;
using namespace phu::sequencing::constants;

TEST_CASE("PatternDocument: default colors follow the palette in lane order", "[M21][REQ-4-5][REQ-20-4]") {
    std::set<std::uint32_t> distinct;
    for (int i = 0; i < lanes::laneCount; ++i) {
        const auto color = defaultLaneColor(i);
        const std::uint32_t rgb = (std::uint32_t{color.red} << 16) | (std::uint32_t{color.green} << 8) | color.blue;
        CHECK(rgb == colors::defaultLanePalette[static_cast<std::size_t>(i)]);
        distinct.insert(rgb);
        CHECK(PatternDocument{}.lane(i).color == color);
    }
    CHECK(distinct.size() == static_cast<std::size_t>(lanes::laneCount));
}

TEST_CASE("PatternDocument: changing one lane color leaves the others unchanged", "[M21][REQ-4-6]") {
    PatternDocument document;
    const PatternDocument before = document;
    document.setLaneColor(2, LaneColor{1, 2, 3});
    CHECK(document.lane(2).color == LaneColor{1, 2, 3});
    for (int i = 0; i < lanes::laneCount; ++i) {
        if (i != 2) {
            CHECK(document.lane(i).color == before.lane(i).color);
        }
    }
}

TEST_CASE("PatternDocument: an invalid text grid is stored but not applied", "[M21][REQ-5.3-2]") {
    PatternDocument document;
    REQUIRE(document.setTextGridText(0, "1/8, 1/4").isValid());
    const auto applied = document.lane(0).pattern.appliedTextGrid;
    CHECK(applied.entryCount == 2);

    CHECK_FALSE(document.setTextGridText(0, "1/8,,").isValid());
    CHECK(document.lane(0).textGridText == "1/8,,");
    CHECK(document.lane(0).pattern.appliedTextGrid == applied);
}

TEST_CASE("PatternDocument: step count invariant resets slots beyond the effective count", "[M21][REQ-5.4-1]") {
    PatternDocument document;
    document.setStepEnabled(0, 20, false);
    document.setStepValue(0, 20, VelocityMode::byStep, 5);
    document.setStepEnabled(0, 3, false);

    LaneParameters parameters;
    parameters.stepCount = 16;
    document.enforceStepCountInvariant(0, parameters);

    CHECK(document.lane(0).pattern.steps.step(20) == defaultStepData());
    CHECK_FALSE(document.lane(0).pattern.steps.step(3).enabled);
}

TEST_CASE("PatternDocument: mutators increase the revision and out-of-range indices are ignored", "[M21]") {
    PatternDocument document;
    const auto revision = document.revision();
    document.setStepEnabled(0, 1, false);
    CHECK(document.revision() > revision);

    const PatternDocument before = document;
    document.setStepEnabled(-1, 0, false);
    document.setStepEnabled(0, lanes::maximumStepCount, false);
    document.setLaneColor(lanes::laneCount, LaneColor{9, 9, 9});
    CHECK(document == before);
}
