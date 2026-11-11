#include <catch2/catch_test_macros.hpp>
#include "sequencing/MidiEvent.h"

using namespace phu::sequencing;

// M06 MIDI events (§3.6, #7)

TEST_CASE("MidiEvent::makeNoteOn creates a Note-On event with given pitch and velocity",
          "[M06][REQ-3.1-1][REQ-3.1-3]") {
    const auto noteOn = makeNoteOn(100, 3, 60, 64);
    CHECK(noteOn.sampleOffset == 100);
    CHECK(noteOn.kind == MidiEventKind::noteOn);
    CHECK(noteOn.channel == 3);
    CHECK(noteOn.data1 == 60);
    CHECK(noteOn.data2 == 64);
}

TEST_CASE("MidiEvent::makeNoteOff creates a Note-Off event with velocity 0", "[M06][REQ-3.1-1]") {
    const auto noteOff = makeNoteOff(200, 5, 67);
    CHECK(noteOff.sampleOffset == 200);
    CHECK(noteOff.kind == MidiEventKind::noteOff);
    CHECK(noteOff.channel == 5);
    CHECK(noteOff.data1 == 67);
    CHECK(noteOff.data2 == 0);
}

TEST_CASE("MidiEvent::normalizeNoteEvent converts Note-On velocity 0 to Note-Off", "[M06][REQ-3.1-2]") {
    const auto noteOnVel0 = makeNoteOn(50, 1, 60, 0);
    const auto normalized = normalizeNoteEvent(noteOnVel0);
    CHECK(normalized.kind == MidiEventKind::noteOff);
    CHECK(normalized.channel == 1);
    CHECK(normalized.data1 == 60);
    CHECK(normalized.data2 == 0);
    CHECK(normalized.sampleOffset == 50);
}

TEST_CASE("MidiEvent::normalizeNoteEvent leaves other events unchanged", "[M06]") {
    // Note-Off should remain unchanged
    const auto noteOff = makeNoteOff(100, 2, 64);
    const auto normalized = normalizeNoteEvent(noteOff);
    CHECK(normalized.kind == MidiEventKind::noteOff);
    CHECK(normalized == noteOff);

    // Note-On with velocity > 0 should remain unchanged
    const auto noteOnVel = makeNoteOn(150, 3, 72, 100);
    const auto normalizedVel = normalizeNoteEvent(noteOnVel);
    CHECK(normalizedVel.kind == MidiEventKind::noteOn);
    CHECK(normalizedVel == noteOnVel);
}

TEST_CASE("MidiEvent::isNoteRelease returns true for Note-Off", "[M06][REQ-3.1-2]") {
    const auto noteOff = makeNoteOff(100, 1, 60);
    CHECK(noteOff.isNoteRelease() == true);
}

TEST_CASE("MidiEvent::isNoteRelease returns true for Note-On with velocity 0", "[M06][REQ-3.1-2]") {
    const auto noteOnVel0 = makeNoteOn(100, 1, 60, 0);
    CHECK(noteOnVel0.isNoteRelease() == true);
}

TEST_CASE("MidiEvent::isNoteRelease returns false for Note-On with velocity > 0", "[M06][REQ-3.1-1]") {
    const auto noteOn = makeNoteOn(100, 1, 60, 64);
    CHECK(noteOn.isNoteRelease() == false);

    // Also test velocity 1 (minimum non-zero)
    const auto noteOnVel1 = makeNoteOn(100, 1, 60, 1);
    CHECK(noteOnVel1.isNoteRelease() == false);
}

TEST_CASE("MidiEvent::isNoteOnWithVelocity returns true for Note-On with velocity > 0",
          "[M06][REQ-3.1-1]") {
    const auto noteOn = makeNoteOn(100, 2, 64, 100);
    CHECK(noteOn.isNoteOnWithVelocity() == true);

    // Also test edge: velocity 1
    const auto noteOnVel1 = makeNoteOn(100, 2, 64, 1);
    CHECK(noteOnVel1.isNoteOnWithVelocity() == true);
}

TEST_CASE("MidiEvent::isNoteOnWithVelocity returns false for Note-Off", "[M06]") {
    const auto noteOff = makeNoteOff(100, 1, 60);
    CHECK(noteOff.isNoteOnWithVelocity() == false);
}

TEST_CASE("MidiEvent::isNoteOnWithVelocity returns false for Note-On velocity 0", "[M06][REQ-3.1-2]") {
    const auto noteOnVel0 = makeNoteOn(100, 1, 60, 0);
    CHECK(noteOnVel0.isNoteOnWithVelocity() == false);
}

TEST_CASE("MidiEvent::makeNoteOn and makeNoteOff respect channel range 1-16", "[M06]") {
    // Channel 1 (minimum)
    const auto ch1On = makeNoteOn(0, 1, 60, 100);
    CHECK(ch1On.channel == 1);

    const auto ch1Off = makeNoteOff(0, 1, 60);
    CHECK(ch1Off.channel == 1);

    // Channel 16 (maximum)
    const auto ch16On = makeNoteOn(0, 16, 127, 127);
    CHECK(ch16On.channel == 16);

    const auto ch16Off = makeNoteOff(0, 16, 127);
    CHECK(ch16Off.channel == 16);
}

TEST_CASE("MidiEvent::makeNoteOn respects pitch range 0-127", "[M06]") {
    // Pitch 0 (minimum)
    const auto pitch0 = makeNoteOn(0, 1, 0, 100);
    CHECK(pitch0.data1 == 0);

    // Pitch 127 (maximum)
    const auto pitch127 = makeNoteOn(0, 1, 127, 100);
    CHECK(pitch127.data1 == 127);
}

TEST_CASE("MidiEvent::makeNoteOn respects velocity range 0-127", "[M06]") {
    // Velocity 0 (minimum, normalized to release)
    const auto vel0 = makeNoteOn(0, 1, 60, 0);
    CHECK(vel0.data2 == 0);

    // Velocity 127 (maximum)
    const auto vel127 = makeNoteOn(0, 1, 60, 127);
    CHECK(vel127.data2 == 127);
}

TEST_CASE("MidiEvent equality compares all fields", "[M06]") {
    const auto event1 = makeNoteOn(100, 1, 60, 64);
    const auto event2 = makeNoteOn(100, 1, 60, 64);
    const auto event3 = makeNoteOn(100, 1, 60, 65);
    const auto event4 = makeNoteOn(101, 1, 60, 64);

    CHECK(event1 == event2);
    CHECK_FALSE(event1 == event3);
    CHECK_FALSE(event1 == event4);
}
