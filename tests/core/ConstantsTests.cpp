#include <catch2/catch_test_macros.hpp>
#include "sequencing/Constants.h"

// M01 Constants (§3.1, §16.3, §20).
using namespace phu::sequencing::constants;

TEST_CASE("Constants: MIDI values are the spec-mandated limits", "[M01][constants][midi]") {
    CHECK(midi::channelCount == 16);
    CHECK(midi::firstChannel == 1);
    CHECK(midi::lastChannel == 16);
    CHECK(midi::lowestPitch == 0);
    CHECK(midi::highestPitch == 127);
    CHECK(midi::pitchCount == 128);
    CHECK(midi::lowestVelocity == 0);
    CHECK(midi::highestVelocity == 127);
    CHECK(midi::noteOffVelocity == 0);
}

TEST_CASE("Constants: velocity limits are defined in exactly one place", "[M01][constants][velocity]") {
    CHECK(velocity::minimumDelta == -127);
    CHECK(velocity::maximumDelta == 127);
    CHECK(velocity::minimumPercent == 0);
    CHECK(velocity::maximumPercent == 200);
    CHECK(velocity::percentDivisor == 100);
}

TEST_CASE("Constants: lane and step limits match §4, §5.4, §9.4", "[M01][constants][lanes]") {
    CHECK(lanes::laneCount == 8);
    CHECK(lanes::minimumStepCount == 1);
    CHECK(lanes::maximumStepCount == 64);
    CHECK(lanes::defaultStepCount == 16);
    CHECK(lanes::minimumFollowIndex == 0);
    CHECK(lanes::maximumFollowIndex == 15);
    CHECK(lanes::minimumGatePercent == 1);
    CHECK(lanes::maximumGatePercent == 200);
    CHECK(lanes::minimumTranspose == -24);
    CHECK(lanes::maximumTranspose == 24);
    CHECK(lanes::minimumSwingPercent == 0);
    CHECK(lanes::maximumSwingPercent == 100);
    CHECK(lanes::swingPercentToShiftDivisor == 200);
    CHECK(lanes::maximumVoicesPerLane == 32);
}

TEST_CASE("Constants: timing list and modifier factors (§5.1, §16.2)", "[M01][constants][timing]") {
    CHECK(timing::baseTimingDenominators == std::array<int, 6>{32, 16, 8, 4, 2, 1});
    CHECK(timing::tripletFactor.numerator == 2);
    CHECK(timing::tripletFactor.denominator == 3);
    CHECK(timing::dottedFactor.numerator == 3);
    CHECK(timing::dottedFactor.denominator == 2);
    CHECK(timing::straightFactor.numerator == 1);
    CHECK(timing::straightFactor.denominator == 1);
    CHECK(timing::textGridNumerator == 1);
    CHECK(timing::textGridDenominators == std::array<int, 7>{1, 2, 4, 8, 16, 32, 64});
    CHECK(timing::textGridMaximumEntries == 64);
}

TEST_CASE("Constants: lane color palette has 8 distinct colors in lane order (§14, §16.3, §20)",
          "[M01][constants][colors]") {
    CHECK(colors::defaultLanePalette.size() == lanes::laneCount);
    for (std::size_t i = 0; i < colors::defaultLanePalette.size(); ++i) {
        for (std::size_t j = i + 1; j < colors::defaultLanePalette.size(); ++j) {
            CHECK(colors::defaultLanePalette[i] != colors::defaultLanePalette[j]);
        }
    }
}

TEST_CASE("Constants: minimum step display width is a named constant (§14, §16.3)",
          "[M01][constants][editor]") {
    CHECK(editor::minimumStepDisplayWidth > 0);
}
