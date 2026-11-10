#include <catch2/catch_test_macros.hpp>
#include "sequencing/NoteValues.h"
#include "sequencing/Constants.h"
#include <array>

// M02 Note values: data-driven subdivisions and base timings (§5.1, §16.2).
using namespace phu::sequencing;

TEST_CASE("NoteValues: subdivisionRegistry has straight, triplet, dotted with correct factors",
          "[M02][note-values][subdivision]") {
    REQUIRE(subdivisionRegistry.size() == 3);
    CHECK(subdivisionRegistry[0].name == "Straight");
    CHECK(subdivisionRegistry[0].factor.numerator == 1);
    CHECK(subdivisionRegistry[0].factor.denominator == 1);
    CHECK(subdivisionRegistry[1].name == "Triplet");
    CHECK(subdivisionRegistry[1].factor.numerator == 2);
    CHECK(subdivisionRegistry[1].factor.denominator == 3);
    CHECK(subdivisionRegistry[2].name == "Dotted");
    CHECK(subdivisionRegistry[2].factor.numerator == 3);
    CHECK(subdivisionRegistry[2].factor.denominator == 2);
}

TEST_CASE("NoteValues: baseTimingRegistry lists the 6 base timings in declared order",
          "[M02][note-values][base-timing]") {
    const auto registry = baseTimingRegistry();
    REQUIRE(registry.size() == 6);
    const std::array<int, 6> expectedDenominators{32, 16, 8, 4, 2, 1};
    const std::array<std::string_view, 6> expectedNames{"1/32", "1/16", "1/8", "1/4", "1/2", "1/1"};
    for (std::size_t i = 0; i < registry.size(); ++i) {
        CHECK(registry[i].denominator == expectedDenominators[i]);
        CHECK(registry[i].name == expectedNames[i]);
    }
}

TEST_CASE("NoteValues: noteLengthTicks is exact for all 6 timings x 3 modifiers",
          "[M02][note-values][REQ-5.1-timing-modifiers]") {
    // Expected values derived from ticksPerWholeNote = 3840: straight = 3840/denominator,
    // triplet = straight*2/3, dotted = straight*3/2 (all exact integers per §16.2's static_asserts).
    struct Expectation { int denominator; TickCount straight; TickCount triplet; TickCount dotted; };
    constexpr std::array<Expectation, 6> expectations{{
        {32, 120, 80, 180},
        {16, 240, 160, 360},
        {8, 480, 320, 720},
        {4, 960, 640, 1440},
        {2, 1920, 1280, 2880},
        {1, 3840, 2560, 5760},
    }};

    for (const auto& expectation : expectations) {
        CAPTURE(expectation.denominator);
        CHECK(noteLengthTicks(NoteLength{expectation.denominator, Subdivision::straight}) == expectation.straight);
        CHECK(noteLengthTicks(NoteLength{expectation.denominator, Subdivision::triplet}) == expectation.triplet);
        CHECK(noteLengthTicks(NoteLength{expectation.denominator, Subdivision::dotted}) == expectation.dotted);
    }
}

TEST_CASE("NoteValues: baseTimingNoteLength maps index to denominator and clamps out-of-range index",
          "[M02][note-values][base-timing]") {
    CHECK(baseTimingNoteLength(0, Subdivision::straight).denominator == 32);
    CHECK(baseTimingNoteLength(1, Subdivision::straight).denominator == 16);
    CHECK(baseTimingNoteLength(5, Subdivision::straight).denominator == 1);
    CHECK(baseTimingNoteLength(-1, Subdivision::straight).denominator == 32);  // clamped low
    CHECK(baseTimingNoteLength(99, Subdivision::straight).denominator == 1);   // clamped high
    CHECK(baseTimingNoteLength(1, Subdivision::triplet).subdivision == Subdivision::triplet);
}

TEST_CASE("NoteValues: NoteLength equality compares denominator and subdivision",
          "[M02][note-values]") {
    CHECK(NoteLength{16, Subdivision::straight} == NoteLength{16, Subdivision::straight});
    CHECK_FALSE(NoteLength{16, Subdivision::straight} == NoteLength{16, Subdivision::triplet});
    CHECK_FALSE(NoteLength{16, Subdivision::straight} == NoteLength{8, Subdivision::straight});
}
