#include <catch2/catch_test_macros.hpp>
#include "sequencing/MusicalTime.h"
#include "sequencing/Constants.h"

// M02 Musical time (§16.2).
using namespace phu::sequencing;

TEST_CASE("MusicalTime: ticksToBeats converts using ticksPerBeat", "[M02][musical-time]") {
    CHECK(ticksToBeats(0) == 0.0);
    CHECK(ticksToBeats(constants::timing::ticksPerBeat) == 1.0);
    CHECK(ticksToBeats(constants::timing::ticksPerBeat * 4) == 4.0);
    CHECK(ticksToBeats(constants::timing::ticksPerBeat / 2) == 0.5);
}

TEST_CASE("MusicalTime: wholeBeatsToTicks converts using ticksPerBeat", "[M02][musical-time]") {
    CHECK(wholeBeatsToTicks(0) == 0);
    CHECK(wholeBeatsToTicks(1) == constants::timing::ticksPerBeat);
    CHECK(wholeBeatsToTicks(4) == constants::timing::ticksPerBeat * 4);
}

TEST_CASE("MusicalTime: ticksToBeats and wholeBeatsToTicks round-trip for whole beats",
          "[M02][musical-time]") {
    for (std::int64_t beats = 0; beats < 8; ++beats) {
        CHECK(ticksToBeats(wholeBeatsToTicks(beats)) == static_cast<Beats>(beats));
    }
}
