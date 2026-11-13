#include <catch2/catch_test_macros.hpp>
#include "arpeggiator/PassThroughFilter.h"
#include "arpeggiator/LaneConfiguration.h"
#include "sequencing/VoiceRegistry.h"
#include "sequencing/OutputEventBuffer.h"
#include "sequencing/MidiEvent.h"
#include "sequencing/Constants.h"

// M16 Pass-through filter: Thru/Consume policy, per-note Note-Off tracking that survives
// later parameter changes, CC64 handling, and the bypass raw-note adoption lifecycle
// (sec. 3.3, 21, arch. 4.5, #11).

using namespace phu::arpeggiator;
using namespace phu::sequencing;

namespace {

int countEventsOfKind(const OutputEventBuffer& buffer, MidiEventKind kind) noexcept {
    int count = 0;
    for (const auto& event : buffer.events()) {
        if (event.kind == kind) {
            ++count;
        }
    }
    return count;
}

GlobalParameters makeGlobal(bool thru, bool consume, int chordInputChannel = 1) {
    GlobalParameters global;
    global.thru = thru;
    global.consume = consume;
    global.chordInputChannel = chordInputChannel;
    return global;
}

}  // namespace

// --- PassThroughPolicy (REQ-3.3-1..3) ---------------------------------------------------

TEST_CASE("PassThroughPolicy: Thru off blocks every channel", "[PassThroughFilter]") {
    std::array<LaneParameters, constants::lanes::laneCount> lanes{};
    auto policy = makePassThroughPolicy(makeGlobal(false, true), lanes);
    for (int ch = 1; ch <= constants::midi::channelCount; ++ch) {
        CHECK_FALSE(policy.passesNoteOn(ch));
    }
}

TEST_CASE("PassThroughPolicy: Thru on, Consume off passes all 16 channels (REQ-3.3-1)", "[PassThroughFilter]") {
    std::array<LaneParameters, constants::lanes::laneCount> lanes{};
    auto policy = makePassThroughPolicy(makeGlobal(true, false), lanes);
    for (int ch = 1; ch <= constants::midi::channelCount; ++ch) {
        CHECK(policy.passesNoteOn(ch));
    }
}

TEST_CASE("PassThroughPolicy: Consume on blocks followed channels even with Thru on (REQ-3.3-2)", "[PassThroughFilter]") {
    std::array<LaneParameters, constants::lanes::laneCount> lanes{};
    lanes[1].inputMode = InputMode::channel;
    lanes[1].followChannel = 3;
    auto policy = makePassThroughPolicy(makeGlobal(true, true, /*chordInputChannel*/ 1), lanes);
    CHECK_FALSE(policy.passesNoteOn(1));  // chord input channel is followed
    CHECK_FALSE(policy.passesNoteOn(3));  // lane 2's follow channel is followed
    CHECK(policy.passesNoteOn(2));        // not followed by anything
}

TEST_CASE("PassThroughPolicy: Consume off lets followed channels pass when Thru on (REQ-3.3-3)", "[PassThroughFilter]") {
    std::array<LaneParameters, constants::lanes::laneCount> lanes{};
    lanes[1].inputMode = InputMode::channel;
    lanes[1].followChannel = 3;
    auto policy = makePassThroughPolicy(makeGlobal(true, false, /*chordInputChannel*/ 1), lanes);
    CHECK(policy.passesNoteOn(1));
    CHECK(policy.passesNoteOn(3));
}

TEST_CASE("PassThroughPolicy: only CURRENT channel-mode lanes contribute to followed channels (D-25)", "[PassThroughFilter]") {
    std::array<LaneParameters, constants::lanes::laneCount> lanes{};
    lanes[1].inputMode = InputMode::chordFollow;  // not channel mode: its followChannel must not count
    lanes[1].followChannel = 5;
    auto policy = makePassThroughPolicy(makeGlobal(true, true, /*chordInputChannel*/ 1), lanes);
    CHECK(policy.passesNoteOn(5));
}

// --- PassThroughFilter::process (REQ-3.3-4..6) ------------------------------------------

TEST_CASE("PassThroughFilter: passed Note-On is remembered and later ends the voice (REQ-3.3-4)", "[PassThroughFilter]") {
    PassThroughFilter filter;
    VoiceRegistry voices;
    OutputEventBuffer output{constants::capacity::outputEventsPerBlock};
    std::array<LaneParameters, constants::lanes::laneCount> lanes{};
    auto policy = makePassThroughPolicy(makeGlobal(true, false), lanes);

    filter.process(makeNoteOn(0, 1, 60, 100), policy, voices, output);
    REQUIRE(countEventsOfKind(output, MidiEventKind::noteOn) == 1);
    REQUIRE(voices.soundingCount(passThroughOwner) == 1);

    output.clear();
    filter.process(makeNoteOff(10, 1, 60), policy, voices, output);
    CHECK(countEventsOfKind(output, MidiEventKind::noteOff) == 1);
    CHECK(voices.soundingCount(passThroughOwner) == 0);
}

TEST_CASE("PassThroughFilter: a Note-Off is suppressed if its Note-On was blocked (REQ-3.3-4 reverse)", "[PassThroughFilter]") {
    PassThroughFilter filter;
    VoiceRegistry voices;
    OutputEventBuffer output{constants::capacity::outputEventsPerBlock};
    std::array<LaneParameters, constants::lanes::laneCount> lanes{};

    auto blockingPolicy = makePassThroughPolicy(makeGlobal(false, true), lanes);
    filter.process(makeNoteOn(0, 1, 60, 100), blockingPolicy, voices, output);
    REQUIRE(output.size() == 0);

    // Thru switches on afterwards, but no handle was ever remembered for this key.
    auto passingPolicy = makePassThroughPolicy(makeGlobal(true, false), lanes);
    output.clear();
    filter.process(makeNoteOff(10, 1, 60), passingPolicy, voices, output);
    CHECK(output.size() == 0);
}

TEST_CASE("PassThroughFilter: tracking survives later Consume/follow-channel changes (REQ-3.3-5)", "[PassThroughFilter]") {
    PassThroughFilter filter;
    VoiceRegistry voices;
    OutputEventBuffer output{constants::capacity::outputEventsPerBlock};
    std::array<LaneParameters, constants::lanes::laneCount> lanes{};

    auto initialPolicy = makePassThroughPolicy(makeGlobal(true, false), lanes);
    filter.process(makeNoteOn(0, 1, 60, 100), initialPolicy, voices, output);
    REQUIRE(voices.soundingCount(passThroughOwner) == 1);

    // Consume turns on and channel 1 becomes a followed channel (e.g. a lane now follows it).
    lanes[0].inputMode = InputMode::channel;
    lanes[0].followChannel = 1;
    auto laterPolicy = makePassThroughPolicy(makeGlobal(true, true, /*chordInputChannel*/ 1), lanes);
    REQUIRE_FALSE(laterPolicy.passesNoteOn(1));

    output.clear();
    filter.process(makeNoteOff(10, 1, 60), laterPolicy, voices, output);
    CHECK(countEventsOfKind(output, MidiEventKind::noteOff) == 1);
    CHECK(voices.soundingCount(passThroughOwner) == 0);
}

TEST_CASE("PassThroughFilter: sustain pedal (CC64) does not alter Note-Off pass-through (REQ-3.3-6)", "[PassThroughFilter]") {
    PassThroughFilter filter;
    VoiceRegistry voices;
    OutputEventBuffer output{constants::capacity::outputEventsPerBlock};
    std::array<LaneParameters, constants::lanes::laneCount> lanes{};
    auto policy = makePassThroughPolicy(makeGlobal(true, false), lanes);

    filter.process(makeNoteOn(0, 1, 60, 100), policy, voices, output);
    output.clear();

    MidiEvent sustainOn{5, MidiEventKind::controlChange, 1, 64, 127};
    filter.process(sustainOn, policy, voices, output);
    CHECK(output.size() == 0);  // the filter never outputs non-note events (CC64 included)

    output.clear();
    filter.process(makeNoteOff(10, 1, 60), policy, voices, output);
    CHECK(countEventsOfKind(output, MidiEventKind::noteOff) == 1);  // Note-Off still passes immediately
}

// --- Bypass raw-note tracking (REQ-21-2..5) ---------------------------------------------

TEST_CASE("PassThroughFilter: clearTracking forgets remembered handles", "[PassThroughFilter]") {
    PassThroughFilter filter;
    VoiceRegistry voices;
    OutputEventBuffer output{constants::capacity::outputEventsPerBlock};
    std::array<LaneParameters, constants::lanes::laneCount> lanes{};
    auto policy = makePassThroughPolicy(makeGlobal(true, false), lanes);

    filter.process(makeNoteOn(0, 1, 60, 100), policy, voices, output);
    REQUIRE(voices.soundingCount(passThroughOwner) == 1);

    // Caller releases the voices (e.g. bypass enter / transport stop) and clears tracking.
    voices.releaseAll(0, output);
    filter.clearTracking();

    output.clear();
    filter.process(makeNoteOff(5, 1, 60), policy, voices, output);
    CHECK(output.size() == 0);  // no handle remembered -> suppressed
}

TEST_CASE("PassThroughFilter: trackRawNote records held keys during bypass", "[PassThroughFilter]") {
    PassThroughFilter filter;
    filter.trackRawNote(makeNoteOn(0, 1, 60, 100));
    filter.trackRawNote(makeNoteOn(0, 1, 64, 90));
    filter.trackRawNote(makeNoteOff(0, 1, 60));  // 60 released again while still bypassed

    VoiceRegistry voices;
    filter.adoptRawNotes(voices);

    CHECK(voices.ownerOf(1, 64) == std::optional<OwnerId>(passThroughOwner));
    CHECK_FALSE(voices.ownerOf(1, 60).has_value());  // released before bypass ended: not adopted
}

TEST_CASE("PassThroughFilter: adoptRawNotes adopts without emitting output (REQ-21-4)", "[PassThroughFilter]") {
    PassThroughFilter filter;
    filter.trackRawNote(makeNoteOn(0, 1, 64, 90));

    VoiceRegistry voices;
    OutputEventBuffer output{constants::capacity::outputEventsPerBlock};
    filter.adoptRawNotes(voices);
    CHECK(output.size() == 0);  // adoption itself never writes to an output buffer
    CHECK(voices.soundingCount(passThroughOwner) == 1);
}

TEST_CASE("PassThroughFilter: a key adopted after bypass passes its later Note-Off (REQ-21-4)", "[PassThroughFilter]") {
    PassThroughFilter filter;
    filter.trackRawNote(makeNoteOn(0, 1, 64, 90));

    VoiceRegistry voices;
    OutputEventBuffer output{constants::capacity::outputEventsPerBlock};
    filter.adoptRawNotes(voices);

    std::array<LaneParameters, constants::lanes::laneCount> lanes{};
    auto policy = makePassThroughPolicy(makeGlobal(false, true), lanes);  // Thru off now: irrelevant to tracked key
    filter.process(makeNoteOff(3, 1, 64), policy, voices, output);
    CHECK(countEventsOfKind(output, MidiEventKind::noteOff) == 1);
}

// --- Retrigger vs pass-through (REQ-9.1-5) ----------------------------------------------

TEST_CASE("PassThroughFilter: a lane retriggering a passed-through key ends it first (REQ-9.1-5)", "[PassThroughFilter]") {
    PassThroughFilter filter;
    VoiceRegistry voices;
    voices.setVoiceLimit(0, constants::lanes::maximumVoicesPerLane);
    OutputEventBuffer output{constants::capacity::outputEventsPerBlock};
    std::array<LaneParameters, constants::lanes::laneCount> lanes{};
    auto policy = makePassThroughPolicy(makeGlobal(true, false), lanes);

    filter.process(makeNoteOn(0, 1, 60, 100), policy, voices, output);
    REQUIRE(voices.ownerOf(1, 60) == std::optional<OwnerId>(passThroughOwner));

    output.clear();
    VoiceStartRequest laneRequest{0, 1, 60, 110, noScheduledEnd};
    voices.startVoice(laneRequest, 20, output);

    // Same sample: retrigger Note-Off (of the passed-through voice) then Note-On (of the lane's).
    REQUIRE(output.size() == 2);
    CHECK(output.events()[0].kind == MidiEventKind::noteOff);
    CHECK(output.events()[1].kind == MidiEventKind::noteOn);
    CHECK(voices.ownerOf(1, 60) == std::optional<OwnerId>(0));

    // The key's later input Note-Off must not double-release the lane's voice: the pass-through
    // filter's remembered handle is now stale (ownership moved to the lane).
    output.clear();
    filter.process(makeNoteOff(30, 1, 60), policy, voices, output);
    CHECK(output.size() == 0);
    CHECK(voices.soundingCount(0) == 1);  // the lane's voice is unaffected
}
