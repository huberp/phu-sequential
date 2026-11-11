#include <catch2/catch_test_macros.hpp>
#include "sequencing/HeldNoteTracker.h"
#include "sequencing/MidiEvent.h"
#include "sequencing/Constants.h"

using namespace phu::sequencing;
using namespace phu::sequencing::constants;

// M07 Held-note tracker (§3.1, §3.2, §4.3, #7)

TEST_CASE("HeldNoteTracker::reset clears all held notes and pedal states", "[M07][REQ-3.1-1]") {
    HeldNoteTracker tracker;

    // Apply some events
    tracker.apply(makeNoteOn(0, 1, 60, 100));
    tracker.apply(makeNoteOn(0, 1, 64, 100));

    // Apply pedal down
    const auto cc64Down = MidiEvent{0, MidiEventKind::controlChange, 1, midi::sustainPedalController, 127};
    tracker.apply(cc64Down);

    // Reset
    tracker.reset();

    // Should have no held notes
    CHECK(tracker.heldCount(1) == 0);
    CHECK(tracker.isSustainPedalDown(1) == false);
}

TEST_CASE("HeldNoteTracker::apply returns true when held set changes on Note-On", "[M07]") {
    HeldNoteTracker tracker;

    // First Note-On should return true
    bool changed = tracker.apply(makeNoteOn(0, 1, 60, 100));
    CHECK(changed == true);

    // Same note with different velocity should return true (replacing)
    changed = tracker.apply(makeNoteOn(0, 1, 60, 90));
    CHECK(changed == true);
}

TEST_CASE("HeldNoteTracker::apply returns false for unrelated events", "[M07]") {
    HeldNoteTracker tracker;

    // Some unrelated CC should return false
    const auto cc1 = MidiEvent{0, MidiEventKind::controlChange, 1, 1, 64};
    bool changed = tracker.apply(cc1);
    CHECK(changed == false);
}

TEST_CASE("HeldNoteTracker: Note-On adds note to chord", "[M07][REQ-3.1-1]") {
    HeldNoteTracker tracker;

    tracker.apply(makeNoteOn(0, 1, 60, 100));
    tracker.apply(makeNoteOn(0, 1, 64, 100));
    tracker.apply(makeNoteOn(0, 1, 67, 100));

    CHECK(tracker.heldCount(1) == 3);
    CHECK(tracker.isHeld(1, 60) == true);
    CHECK(tracker.isHeld(1, 64) == true);
    CHECK(tracker.isHeld(1, 67) == true);
}

TEST_CASE("HeldNoteTracker: Note-Off removes note from chord", "[M07][REQ-3.1-1]") {
    HeldNoteTracker tracker;

    tracker.apply(makeNoteOn(0, 1, 60, 100));
    tracker.apply(makeNoteOn(0, 1, 64, 100));
    tracker.apply(makeNoteOn(0, 1, 67, 100));

    tracker.apply(makeNoteOff(0, 1, 64));

    CHECK(tracker.heldCount(1) == 2);
    CHECK(tracker.isHeld(1, 60) == true);
    CHECK(tracker.isHeld(1, 64) == false);
    CHECK(tracker.isHeld(1, 67) == true);
}

TEST_CASE("HeldNoteTracker: Note-On with velocity 0 acts as Note-Off", "[M07][REQ-3.1-2]") {
    HeldNoteTracker tracker;

    tracker.apply(makeNoteOn(0, 1, 60, 100));
    CHECK(tracker.isHeld(1, 60) == true);

    tracker.apply(makeNoteOn(0, 1, 60, 0));
    CHECK(tracker.isHeld(1, 60) == false);
}

TEST_CASE("HeldNoteTracker: Repeat Note-On replaces velocity (size stays 1)", "[M07][REQ-3.1-3]") {
    HeldNoteTracker tracker;

    tracker.apply(makeNoteOn(0, 1, 60, 50));
    CHECK(tracker.heldCount(1) == 1);
    const auto note1 = tracker.heldNoteAtIndex(1, 0);
    REQUIRE(note1.has_value());
    CHECK(note1->velocity == 50);

    tracker.apply(makeNoteOn(0, 1, 60, 90));
    CHECK(tracker.heldCount(1) == 1);  // Size should stay 1
    const auto note2 = tracker.heldNoteAtIndex(1, 0);
    REQUIRE(note2.has_value());
    CHECK(note2->velocity == 90);  // Velocity should be updated
}

TEST_CASE("HeldNoteTracker: Pedal on (CC64 >= 64) holds notes after Note-Off",
          "[M07][REQ-3.1-4][REQ-3.1-7]") {
    HeldNoteTracker tracker;

    // Pedal on
    const auto cc64On = MidiEvent{0, MidiEventKind::controlChange, 1, midi::sustainPedalController, 127};
    tracker.apply(cc64On);

    // Note on, then off
    tracker.apply(makeNoteOn(0, 1, 60, 100));
    tracker.apply(makeNoteOff(0, 1, 60));

    // Note should still be held due to pedal
    CHECK(tracker.isHeld(1, 60) == true);
    CHECK(tracker.heldCount(1) == 1);
}

TEST_CASE("HeldNoteTracker: Pedal off (CC64 < 64) releases sustained notes", "[M07][REQ-3.1-5]") {
    HeldNoteTracker tracker;

    // Pedal on
    const auto cc64On = MidiEvent{0, MidiEventKind::controlChange, 1, midi::sustainPedalController, 127};
    tracker.apply(cc64On);

    // Note on, then off
    tracker.apply(makeNoteOn(0, 1, 60, 100));
    tracker.apply(makeNoteOff(0, 1, 60));

    // Note should be held due to pedal
    CHECK(tracker.isHeld(1, 60) == true);

    // Pedal off
    const auto cc64Off = MidiEvent{0, MidiEventKind::controlChange, 1, midi::sustainPedalController, 63};
    tracker.apply(cc64Off);

    // Note should now be released
    CHECK(tracker.isHeld(1, 60) == false);
    CHECK(tracker.heldCount(1) == 0);
}

TEST_CASE("HeldNoteTracker: Pedal threshold is 63 (off) / 64 (on)", "[M07][REQ-3.1-7]") {
    HeldNoteTracker tracker;

    // CC64 = 63: no hold
    const auto cc64_63 = MidiEvent{0, MidiEventKind::controlChange, 1, midi::sustainPedalController, 63};
    tracker.apply(cc64_63);
    tracker.apply(makeNoteOn(0, 1, 60, 100));
    tracker.apply(makeNoteOff(0, 1, 60));

    CHECK(tracker.isHeld(1, 60) == false);

    // Reset and try again with CC64 = 64: hold
    tracker.reset();

    const auto cc64_64 = MidiEvent{0, MidiEventKind::controlChange, 1, midi::sustainPedalController, 64};
    tracker.apply(cc64_64);
    tracker.apply(makeNoteOn(0, 1, 60, 100));
    tracker.apply(makeNoteOff(0, 1, 60));

    CHECK(tracker.isHeld(1, 60) == true);
}

TEST_CASE("HeldNoteTracker: Pedal is per-channel", "[M07][REQ-3.1-4]") {
    HeldNoteTracker tracker;

    // Pedal on for channel 1
    const auto cc64On_ch1 = MidiEvent{0, MidiEventKind::controlChange, 1, midi::sustainPedalController, 127};
    tracker.apply(cc64On_ch1);

    // Note on ch1 and ch2, both off
    tracker.apply(makeNoteOn(0, 1, 60, 100));
    tracker.apply(makeNoteOn(0, 2, 60, 100));
    tracker.apply(makeNoteOff(0, 1, 60));
    tracker.apply(makeNoteOff(0, 2, 60));

    // Ch1 note held (has pedal), ch2 note not held (no pedal)
    CHECK(tracker.isHeld(1, 60) == true);
    CHECK(tracker.isHeld(2, 60) == false);
}

TEST_CASE("HeldNoteTracker::isSustainPedalDown reports pedal state per channel", "[M07][REQ-3.1-4]") {
    HeldNoteTracker tracker;

    // Initially pedal up
    CHECK(tracker.isSustainPedalDown(1) == false);

    // Pedal on for ch1
    const auto cc64On = MidiEvent{0, MidiEventKind::controlChange, 1, midi::sustainPedalController, 127};
    tracker.apply(cc64On);

    CHECK(tracker.isSustainPedalDown(1) == true);
    CHECK(tracker.isSustainPedalDown(2) == false);

    // Pedal off for ch1
    const auto cc64Off = MidiEvent{0, MidiEventKind::controlChange, 1, midi::sustainPedalController, 63};
    tracker.apply(cc64Off);

    CHECK(tracker.isSustainPedalDown(1) == false);
}

TEST_CASE("HeldNoteTracker::heldNoteAtIndex returns notes in ascending pitch order (REQ-3.4-1)",
          "[M07][REQ-3.4-1]") {
    HeldNoteTracker tracker;

    // Add notes in descending order
    tracker.apply(makeNoteOn(0, 1, 67, 100));
    tracker.apply(makeNoteOn(0, 1, 60, 100));
    tracker.apply(makeNoteOn(0, 1, 64, 100));

    // Check sorted order
    const auto note0 = tracker.heldNoteAtIndex(1, 0);
    const auto note1 = tracker.heldNoteAtIndex(1, 1);
    const auto note2 = tracker.heldNoteAtIndex(1, 2);

    REQUIRE(note0.has_value());
    REQUIRE(note1.has_value());
    REQUIRE(note2.has_value());

    CHECK(note0->pitch == 60);  // Lowest
    CHECK(note1->pitch == 64);  // Middle
    CHECK(note2->pitch == 67);  // Highest
}

TEST_CASE("HeldNoteTracker::heldNoteAtIndex returns nullopt for index >= size", "[M07][REQ-4.2-4]") {
    HeldNoteTracker tracker;

    tracker.apply(makeNoteOn(0, 1, 60, 100));
    tracker.apply(makeNoteOn(0, 1, 64, 100));
    tracker.apply(makeNoteOn(0, 1, 67, 100));

    CHECK(tracker.heldNoteAtIndex(1, 0).has_value() == true);
    CHECK(tracker.heldNoteAtIndex(1, 2).has_value() == true);
    CHECK(tracker.heldNoteAtIndex(1, 3).has_value() == false);
    CHECK(tracker.heldNoteAtIndex(1, 15).has_value() == false);
}

TEST_CASE("HeldNoteTracker::heldNoteAtIndex is O(128) rescan", "[M07]") {
    HeldNoteTracker tracker;

    // Add multiple notes
    for (int pitch = 60; pitch <= 72; ++pitch) {
        tracker.apply(makeNoteOn(0, 1, pitch, 100));
    }

    // Access multiple indices
    for (int i = 0; i < 13; ++i) {
        const auto note = tracker.heldNoteAtIndex(1, i);
        REQUIRE(note.has_value());
        CHECK(note->pitch == 60 + i);
    }
}

TEST_CASE("HeldNoteTracker::mostRecentNote returns most recent held note (§4.3)", "[M07][REQ-4.3-1]") {
    HeldNoteTracker tracker;

    tracker.apply(makeNoteOn(0, 1, 60, 100));
    tracker.apply(makeNoteOn(0, 1, 64, 100));

    const auto recent = tracker.mostRecentNote(1);
    REQUIRE(recent.has_value());
    CHECK(recent->pitch == 64);  // Most recent
}

TEST_CASE("HeldNoteTracker::mostRecentNote returns nullopt after release of followed note",
          "[M07][REQ-4.3-2]") {
    HeldNoteTracker tracker;

    tracker.apply(makeNoteOn(0, 1, 60, 100));
    tracker.apply(makeNoteOn(0, 1, 64, 100));

    // Release the most recent
    tracker.apply(makeNoteOff(0, 1, 64));

    // Should be nullopt even though 60 is still held (no note stack)
    const auto recent = tracker.mostRecentNote(1);
    CHECK(recent.has_value() == false);
}

TEST_CASE("HeldNoteTracker::mostRecentNote returns nullopt before any note on channel",
          "[M07][REQ-4.3-3]") {
    HeldNoteTracker tracker;

    const auto recent = tracker.mostRecentNote(1);
    CHECK(recent.has_value() == false);
}

TEST_CASE("HeldNoteTracker::mostRecentNote ignores older notes when newer is held", "[M07][REQ-4.3-4]") {
    HeldNoteTracker tracker;

    tracker.apply(makeNoteOn(0, 1, 60, 100));
    tracker.apply(makeNoteOn(0, 1, 64, 100));

    // Release older, not the most recent
    tracker.apply(makeNoteOff(0, 1, 60));

    // Should still return the most recent
    const auto recent = tracker.mostRecentNote(1);
    REQUIRE(recent.has_value());
    CHECK(recent->pitch == 64);
}

TEST_CASE("HeldNoteTracker::mostRecentNote stays while sustained, ends on pedal up (§3.1, REQ-3.1-6)",
          "[M07][REQ-3.1-6]") {
    HeldNoteTracker tracker;
    tracker.apply(MidiEvent{0, MidiEventKind::controlChange, 1, midi::sustainPedalController, 127});
    tracker.apply(makeNoteOn(0, 1, 60, 100));
    tracker.apply(makeNoteOn(0, 1, 64, 90));
    tracker.apply(makeNoteOff(0, 1, 64));

    REQUIRE(tracker.mostRecentNote(1).has_value());
    CHECK(tracker.mostRecentNote(1)->pitch == 64);

    tracker.apply(MidiEvent{0, MidiEventKind::controlChange, 1, midi::sustainPedalController, 0});
    CHECK_FALSE(tracker.mostRecentNote(1).has_value());
}

TEST_CASE("HeldNoteTracker: release of a never-pressed pitch under pedal is not held", "[M07][REQ-3.1-4]") {
    HeldNoteTracker tracker;
    tracker.apply(MidiEvent{0, MidiEventKind::controlChange, 1, midi::sustainPedalController, 127});
    tracker.apply(makeNoteOff(0, 1, 60));
    CHECK(tracker.heldCount(1) == 0);
}

TEST_CASE("HeldNoteTracker: Out-of-range channel ignored", "[M07][REQ-3.2-2]") {
    HeldNoteTracker tracker;

    // Channel 0 (out of range, 1-16 valid)
    tracker.apply(makeNoteOn(0, 0, 60, 100));

    // Channel 17 (out of range)
    tracker.apply(makeNoteOn(0, 17, 60, 100));

    // Nothing should be held on valid channels
    CHECK(tracker.heldCount(1) == 0);
    CHECK(tracker.isHeld(0, 60) == false);
    CHECK(tracker.isHeld(17, 60) == false);
}

TEST_CASE("HeldNoteTracker: Out-of-range pitch ignored", "[M07]") {
    HeldNoteTracker tracker;

    // Pitch -1 and 128 (out of range 0-127)
    // Note: API uses int, values should be clamped/ignored
    tracker.apply(makeNoteOn(0, 1, 128, 100));
    tracker.apply(makeNoteOn(0, 1, -1, 100));

    // Behavior: implementation should ignore out-of-range pitches
    // For now just check nothing breaks
    CHECK(tracker.heldCount(1) == 0);
}

TEST_CASE("HeldNoteTracker: heldCount returns 0 for empty channel", "[M07]") {
    HeldNoteTracker tracker;

    CHECK(tracker.heldCount(1) == 0);
    CHECK(tracker.heldCount(5) == 0);
    CHECK(tracker.heldCount(16) == 0);
}

TEST_CASE("HeldNoteTracker: Multiple channels independent", "[M07][REQ-3.2-1]") {
    HeldNoteTracker tracker;

    tracker.apply(makeNoteOn(0, 1, 60, 100));
    tracker.apply(makeNoteOn(0, 2, 64, 100));
    tracker.apply(makeNoteOn(0, 3, 67, 100));

    CHECK(tracker.heldCount(1) == 1);
    CHECK(tracker.heldCount(2) == 1);
    CHECK(tracker.heldCount(3) == 1);

    CHECK(tracker.isHeld(1, 60) == true);
    CHECK(tracker.isHeld(2, 64) == true);
    CHECK(tracker.isHeld(3, 67) == true);

    // Cross-check: ch1 should not have ch2's notes
    CHECK(tracker.isHeld(1, 64) == false);
    CHECK(tracker.isHeld(2, 60) == false);
}

TEST_CASE("HeldNoteTracker: Re-sort on release maintains sorted order", "[M07][REQ-3.4-1]") {
    HeldNoteTracker tracker;

    // Add notes
    tracker.apply(makeNoteOn(0, 1, 60, 100));
    tracker.apply(makeNoteOn(0, 1, 67, 100));
    tracker.apply(makeNoteOn(0, 1, 64, 100));

    // Release middle note
    tracker.apply(makeNoteOff(0, 1, 64));

    // Check order is still ascending
    const auto note0 = tracker.heldNoteAtIndex(1, 0);
    const auto note1 = tracker.heldNoteAtIndex(1, 1);

    REQUIRE(note0.has_value());
    REQUIRE(note1.has_value());
    CHECK(note0->pitch == 60);
    CHECK(note1->pitch == 67);
}

TEST_CASE("HeldNoteTracker::apply returns true on Release with pedal active", "[M07]") {
    HeldNoteTracker tracker;

    const auto cc64On = MidiEvent{0, MidiEventKind::controlChange, 1, midi::sustainPedalController, 127};
    tracker.apply(cc64On);

    tracker.apply(makeNoteOn(0, 1, 60, 100));

    // Release while pedal on should return true (held set changes)
    bool changed = tracker.apply(makeNoteOff(0, 1, 60));
    CHECK(changed == true);
}

TEST_CASE("HeldNoteTracker: Pedal down on CC64 value 64 (boundary)", "[M07][REQ-3.1-7]") {
    HeldNoteTracker tracker;

    // Exactly at threshold
    const auto cc64_boundary = MidiEvent{0, MidiEventKind::controlChange, 1, midi::sustainPedalController, 64};
    tracker.apply(cc64_boundary);

    tracker.apply(makeNoteOn(0, 1, 60, 100));
    tracker.apply(makeNoteOff(0, 1, 60));

    CHECK(tracker.isHeld(1, 60) == true);
    CHECK(tracker.isSustainPedalDown(1) == true);
}

TEST_CASE("HeldNoteTracker: Keys held before release still held after pedal off", "[M07][REQ-3.1-5]") {
    HeldNoteTracker tracker;

    // Start with note held (key down)
    tracker.apply(makeNoteOn(0, 1, 60, 100));

    // Turn on pedal
    const auto cc64On = MidiEvent{0, MidiEventKind::controlChange, 1, midi::sustainPedalController, 127};
    tracker.apply(cc64On);

    // Release same note
    tracker.apply(makeNoteOff(0, 1, 60));

    // Note should be sustained (pedal-down)
    CHECK(tracker.isHeld(1, 60) == true);

    // Add another note while pedal is on and first is sustained
    tracker.apply(makeNoteOn(0, 1, 64, 100));

    // Turn off pedal
    const auto cc64Off = MidiEvent{0, MidiEventKind::controlChange, 1, midi::sustainPedalController, 63};
    tracker.apply(cc64Off);

    // First note should be released (was not key-held)
    // But second note should still be held (was key-held)
    CHECK(tracker.isHeld(1, 60) == false);
    CHECK(tracker.isHeld(1, 64) == true);
}

TEST_CASE("HeldNoteTracker: Chord rebuild from held notes after channel change", "[M07][REQ-3.2-2]") {
    HeldNoteTracker tracker;

    // Hold some notes on ch1
    tracker.apply(makeNoteOn(0, 1, 60, 100));
    tracker.apply(makeNoteOn(0, 1, 64, 100));

    // Now query notes from different channels
    CHECK(tracker.heldNoteAtIndex(1, 0).has_value() == true);
    CHECK(tracker.heldNoteAtIndex(2, 0).has_value() == false);

    // Simulate changing to a different channel by checking different channel's notes
    tracker.apply(makeNoteOn(0, 2, 67, 100));

    // Both channels should have their held notes
    CHECK(tracker.heldCount(1) == 2);
    CHECK(tracker.heldCount(2) == 1);
}
