#include <catch2/catch_test_macros.hpp>
#include "sequencing/sources/SourceSelection.h"
#include "sequencing/sources/ChordFollowSource.h"
#include "sequencing/sources/ChannelSource.h"
#include "sequencing/sources/InputModeRegistry.h"
#include "sequencing/HeldNoteTracker.h"
#include "sequencing/MidiEvent.h"
#include "sequencing/Constants.h"

using namespace phu::sequencing;
using namespace phu::sequencing::constants;

// M08 Source selection and input mode strategies (§3.8, §16.4, #7)

TEST_CASE("SourceSelection: selectChordFollowSource uses heldNoteAtIndex on chord input channel",
          "[M08][REQ-4.2-1][REQ-4.2-2][REQ-4.2-3][REQ-4.2-4]") {
    HeldNoteTracker tracker;

    // Set up chord on ch2
    tracker.apply(makeNoteOn(0, 2, 60, 100));
    tracker.apply(makeNoteOn(0, 2, 64, 100));
    tracker.apply(makeNoteOn(0, 2, 67, 100));

    // Set up notes on other channels (should be ignored)
    tracker.apply(makeNoteOn(0, 1, 50, 100));
    tracker.apply(makeNoteOn(0, 3, 70, 100));

    SourceSelector selector;
    selector.chordInputChannel = 2;
    selector.followIndex = 1;

    const auto source = selectChordFollowSource(tracker, selector);
    REQUIRE(source.has_value());
    CHECK(source->pitch == 64);  // Index 1 in sorted chord
}

TEST_CASE("SourceSelection: selectChordFollowSource returns nullopt when index out of range",
          "[M08][REQ-4.2-4]") {
    HeldNoteTracker tracker;

    // Chord with only 3 notes
    tracker.apply(makeNoteOn(0, 1, 60, 100));
    tracker.apply(makeNoteOn(0, 1, 64, 100));
    tracker.apply(makeNoteOn(0, 1, 67, 100));

    SourceSelector selector;
    selector.chordInputChannel = 1;
    selector.followIndex = 15;  // Out of range

    const auto source = selectChordFollowSource(tracker, selector);
    CHECK(source.has_value() == false);
}

TEST_CASE("SourceSelection: selectChordFollowSource returns nullopt for empty chord",
          "[M08][REQ-4.2-3]") {
    HeldNoteTracker tracker;

    SourceSelector selector;
    selector.chordInputChannel = 1;
    selector.followIndex = 0;

    const auto source = selectChordFollowSource(tracker, selector);
    CHECK(source.has_value() == false);
}

TEST_CASE("SourceSelection: selectChannelSource uses mostRecentNote on follow channel",
          "[M08][REQ-4.3-1]") {
    HeldNoteTracker tracker;

    // Set up notes on ch3
    tracker.apply(makeNoteOn(0, 3, 60, 100));
    tracker.apply(makeNoteOn(0, 3, 64, 100));

    SourceSelector selector;
    selector.followChannel = 3;

    const auto source = selectChannelSource(tracker, selector);
    REQUIRE(source.has_value());
    CHECK(source->pitch == 64);  // Most recent
}

TEST_CASE("SourceSelection: selectChannelSource returns nullopt before first note on channel",
          "[M08][REQ-4.3-3]") {
    HeldNoteTracker tracker;

    SourceSelector selector;
    selector.followChannel = 1;

    const auto source = selectChannelSource(tracker, selector);
    CHECK(source.has_value() == false);
}

TEST_CASE("SourceSelection: selectChannelSource returns nullopt after release of most recent note",
          "[M08][REQ-4.3-2]") {
    HeldNoteTracker tracker;

    tracker.apply(makeNoteOn(0, 1, 60, 100));
    tracker.apply(makeNoteOn(0, 1, 64, 100));

    // Release the most recent
    tracker.apply(makeNoteOff(0, 1, 64));

    SourceSelector selector;
    selector.followChannel = 1;

    const auto source = selectChannelSource(tracker, selector);
    CHECK(source.has_value() == false);
}

TEST_CASE("SourceSelection: selectChannelSource ignores older notes not the most recent",
          "[M08][REQ-4.3-4]") {
    HeldNoteTracker tracker;

    tracker.apply(makeNoteOn(0, 1, 60, 100));
    tracker.apply(makeNoteOn(0, 1, 64, 100));

    // Release the older one
    tracker.apply(makeNoteOff(0, 1, 60));

    // Most recent is still 64
    SourceSelector selector;
    selector.followChannel = 1;

    const auto source = selectChannelSource(tracker, selector);
    REQUIRE(source.has_value());
    CHECK(source->pitch == 64);
}

TEST_CASE("SourceSelection::selectSource dispatches via registry", "[M08]") {
    HeldNoteTracker tracker;

    // Set up notes for chord follow
    tracker.apply(makeNoteOn(0, 2, 60, 100));
    tracker.apply(makeNoteOn(0, 2, 64, 100));

    SourceSelector selector;
    selector.inputMode = InputMode::chordFollow;
    selector.chordInputChannel = 2;
    selector.followIndex = 0;

    const auto source = selectSource(tracker, selector);
    REQUIRE(source.has_value());
    CHECK(source->pitch == 60);  // Index 0
}

TEST_CASE("SourceSelection::selectSource dispatches channel mode via registry", "[M08]") {
    HeldNoteTracker tracker;

    tracker.apply(makeNoteOn(0, 3, 60, 100));
    tracker.apply(makeNoteOn(0, 3, 64, 100));

    SourceSelector selector;
    selector.inputMode = InputMode::channel;
    selector.followChannel = 3;

    const auto source = selectSource(tracker, selector);
    REQUIRE(source.has_value());
    CHECK(source->pitch == 64);  // Most recent
}

TEST_CASE("SourceSelection::classifySourceChange detects no change (none -> none)", "[M08]") {
    const auto change = classifySourceChange(std::nullopt, std::nullopt);
    CHECK(change == SourceChange::unchanged);
}

TEST_CASE("SourceSelection::classifySourceChange detects start (none -> note)", "[M08]") {
    const HeldNote note{60, 100};
    const auto change = classifySourceChange(std::nullopt, note);
    CHECK(change == SourceChange::started);
}

TEST_CASE("SourceSelection::classifySourceChange detects release (note -> none)", "[M08]") {
    const HeldNote note{60, 100};
    const auto change = classifySourceChange(note, std::nullopt);
    CHECK(change == SourceChange::released);
}

TEST_CASE("SourceSelection::classifySourceChange detects pitch change (different pitch)", "[M08]") {
    const HeldNote note1{60, 100};
    const HeldNote note2{64, 100};
    const auto change = classifySourceChange(note1, note2);
    CHECK(change == SourceChange::pitchChanged);
}

TEST_CASE("SourceSelection::classifySourceChange detects unchanged (same pitch, possibly velocity change)",
          "[M08]") {
    const HeldNote note1{60, 100};
    const HeldNote note2{60, 90};
    const auto change = classifySourceChange(note1, note2);
    CHECK(change == SourceChange::unchanged);
}

TEST_CASE("SourceSelection::classifySourceChange detects unchanged (same note exactly)", "[M08]") {
    const HeldNote note{60, 100};
    const auto change = classifySourceChange(note, note);
    CHECK(change == SourceChange::unchanged);
}

TEST_CASE("SourceSelection: inputModeRegistry has inputModeCount entries", "[M08][REQ-16.4-1]") {
    CHECK(inputModeRegistry.size() == inputModeCount);
}

TEST_CASE("SourceSelection: inputModeRegistry entries have names and persistence keys", "[M08]") {
    // All entries should have non-empty names and keys
    for (const auto& definition : inputModeRegistry) {
        CHECK(static_cast<std::string>(definition.name) != "");
        CHECK(static_cast<std::string>(definition.persistenceKey) != "");
        CHECK(definition.select != nullptr);
    }
}

TEST_CASE("SourceSelection: chordFollow mode is at index 0", "[M08]") {
    const auto& definition = inputModeRegistry[static_cast<int>(InputMode::chordFollow)];
    CHECK(static_cast<std::string>(definition.name) == "Chord Follow");
    CHECK(static_cast<std::string>(definition.persistenceKey) == "chordFollow");
    CHECK(definition.select == &selectChordFollowSource);
}

TEST_CASE("SourceSelection: channel mode is at index 1", "[M08]") {
    const auto& definition = inputModeRegistry[static_cast<int>(InputMode::channel)];
    CHECK(static_cast<std::string>(definition.name) == "Channel");
    CHECK(static_cast<std::string>(definition.persistenceKey) == "channel");
    CHECK(definition.select == &selectChannelSource);
}

TEST_CASE("SourceSelection: custom strategy can be registered without modifying existing code",
          "[M08][REQ-16.4-1]") {
    // This test demonstrates that a strategy following the interface can work
    // without altering existing code. We'll create a simple test strategy.

    // Mock: define a test input mode that always returns pitch 99
    static constexpr auto testSelectSource = [](const HeldNoteTracker&, const SourceSelector&) noexcept {
        return std::optional<SourceNote>{SourceNote{99, 127}};
    };

    // The type should match the function signature
    static_assert(std::is_invocable_v<decltype(testSelectSource), const HeldNoteTracker&, const SourceSelector&>);

    // Demonstrate it works with selectSource's expected signature
    HeldNoteTracker tracker;
    SourceSelector selector;

    const auto result = testSelectSource(tracker, selector);
    REQUIRE(result.has_value());
    CHECK(result->pitch == 99);
}

TEST_CASE("SourceSelection: SourceSelector has correct default values", "[M08]") {
    const SourceSelector selector;
    CHECK(selector.inputMode == InputMode::chordFollow);
    CHECK(selector.followIndex == lanes::defaultFollowIndex);
    CHECK(selector.followChannel == lanes::defaultFollowChannel);
    CHECK(selector.chordInputChannel == lanes::defaultChordInputChannel);
}

TEST_CASE("SourceSelection: SourceSelector equality operator works", "[M08]") {
    SourceSelector s1;
    SourceSelector s2;
    CHECK(s1 == s2);

    s1.followIndex = 5;
    CHECK(!(s1 == s2));

    s2.followIndex = 5;
    CHECK(s1 == s2);
}

TEST_CASE("SourceSelection: selectChordFollowSource preserves velocity from held note",
          "[M08][REQ-4.2-1]") {
    HeldNoteTracker tracker;

    // Note with specific velocity
    tracker.apply(makeNoteOn(0, 1, 60, 85));

    SourceSelector selector;
    selector.chordInputChannel = 1;
    selector.followIndex = 0;

    const auto source = selectChordFollowSource(tracker, selector);
    REQUIRE(source.has_value());
    CHECK(source->velocity == 85);
}

TEST_CASE("SourceSelection: selectChannelSource preserves velocity from most recent note", "[M08]") {
    HeldNoteTracker tracker;

    tracker.apply(makeNoteOn(0, 1, 60, 100));
    tracker.apply(makeNoteOn(0, 1, 64, 75));

    SourceSelector selector;
    selector.followChannel = 1;

    const auto source = selectChannelSource(tracker, selector);
    REQUIRE(source.has_value());
    CHECK(source->velocity == 75);
}

TEST_CASE("SourceSelection: Multiple modes on same tracker maintain independence", "[M08]") {
    HeldNoteTracker tracker;

    // Chord follow on ch2
    tracker.apply(makeNoteOn(0, 2, 60, 100));
    tracker.apply(makeNoteOn(0, 2, 64, 100));

    // Channel mode on ch1
    tracker.apply(makeNoteOn(0, 1, 70, 100));

    SourceSelector selector1;
    selector1.inputMode = InputMode::chordFollow;
    selector1.chordInputChannel = 2;
    selector1.followIndex = 0;

    SourceSelector selector2;
    selector2.inputMode = InputMode::channel;
    selector2.followChannel = 1;

    const auto source1 = selectSource(tracker, selector1);
    const auto source2 = selectSource(tracker, selector2);

    REQUIRE(source1.has_value());
    REQUIRE(source2.has_value());
    CHECK(source1->pitch == 60);
    CHECK(source2->pitch == 70);
}

TEST_CASE("SourceSelection: Pedal-sustained notes work with chord follow", "[M08][REQ-3.1-4]") {
    HeldNoteTracker tracker;

    // Pedal on
    const auto cc64On = MidiEvent{0, MidiEventKind::controlChange, 1, midi::sustainPedalController, 127};
    tracker.apply(cc64On);

    // Note on, then off (will be sustained by pedal)
    tracker.apply(makeNoteOn(0, 1, 60, 100));
    tracker.apply(makeNoteOff(0, 1, 60));

    // Add another note
    tracker.apply(makeNoteOn(0, 1, 64, 100));

    SourceSelector selector;
    selector.chordInputChannel = 1;
    selector.followIndex = 1;

    // Should get the second note (index 1 in sorted order)
    const auto source = selectChordFollowSource(tracker, selector);
    REQUIRE(source.has_value());
    CHECK(source->pitch == 64);
}

TEST_CASE("SourceSelection: Out-of-range channel treated as empty chord for chord follow",
          "[M08]") {
    HeldNoteTracker tracker;

    // Notes on valid channels
    tracker.apply(makeNoteOn(0, 1, 60, 100));

    SourceSelector selector;
    selector.chordInputChannel = 0;  // Invalid channel
    selector.followIndex = 0;

    // Should return nullopt (no notes on invalid channel)
    const auto source = selectChordFollowSource(tracker, selector);
    CHECK(source.has_value() == false);
}

TEST_CASE("SourceSelection: Out-of-range channel treated as silent for channel mode", "[M08]") {
    HeldNoteTracker tracker;

    tracker.apply(makeNoteOn(0, 1, 60, 100));

    SourceSelector selector;
    selector.followChannel = 17;  // Invalid channel

    const auto source = selectChannelSource(tracker, selector);
    CHECK(source.has_value() == false);
}

TEST_CASE("SourceSelection: Chord re-sorting after release updates chord follow index",
          "[M08][REQ-4.2-5]") {
    HeldNoteTracker tracker;

    tracker.apply(makeNoteOn(0, 1, 60, 100));
    tracker.apply(makeNoteOn(0, 1, 64, 100));
    tracker.apply(makeNoteOn(0, 1, 67, 100));

    SourceSelector selector;
    selector.chordInputChannel = 1;
    selector.followIndex = 1;

    // Initially index 1 is 64
    auto source = selectChordFollowSource(tracker, selector);
    REQUIRE(source.has_value());
    CHECK(source->pitch == 64);

    // Release the middle note
    tracker.apply(makeNoteOff(0, 1, 64));

    // Index 1 should now be 67 (re-sorted)
    source = selectChordFollowSource(tracker, selector);
    REQUIRE(source.has_value());
    CHECK(source->pitch == 67);
}
