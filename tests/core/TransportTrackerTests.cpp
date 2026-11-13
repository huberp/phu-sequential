#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "arpeggiator/TransportTracker.h"

// M15 Transport tracker: start/stop/continuous/discontinuity classification and the
// derived BlockClock window (sec. 10, arch. 4.4, #11). Numbers below use 120 BPM at
// 48000 Hz so samplesPerBeat == 24000 and a 24000-sample block spans exactly one beat.

using namespace phu::arpeggiator;
using Catch::Approx;

namespace {

TransportInfo playing(double playheadBeat, double tempo = 120.0) {
    TransportInfo info;
    info.isPlaying = true;
    info.hasMusicalPosition = true;
    info.playheadBeat = playheadBeat;
    info.tempoBeatsPerMinute = tempo;
    return info;
}

TransportInfo stopped() {
    return TransportInfo{};
}

constexpr double sampleRate = 48000.0;
constexpr int oneBeatBlock = 24000; // 1 beat at 120 BPM, 48000 Hz

}  // namespace

TEST_CASE("TransportTracker: not playing from fresh tracker reports idle", "[TransportTracker]") {
    TransportTracker tracker;
    auto result = tracker.update(stopped(), sampleRate, oneBeatBlock);
    CHECK(result.transition == TransportTransition::idle);
}

TEST_CASE("TransportTracker: first playing block reports started", "[TransportTracker]") {
    TransportTracker tracker;
    auto result = tracker.update(playing(0.0), sampleRate, oneBeatBlock);
    CHECK(result.transition == TransportTransition::started);
    CHECK(result.clock.blockStartBeat == Approx(0.0));
    CHECK(result.clock.windowStartBeat == Approx(0.0));
    CHECK(result.clock.windowEndBeat == Approx(1.0));
    CHECK(result.clock.samplesPerBeat == Approx(24000.0));
}

TEST_CASE("TransportTracker: start sets the phase mapping at an arbitrary PPQ", "[TransportTracker]") {
    TransportTracker tracker;
    auto result = tracker.update(playing(7.3), sampleRate, oneBeatBlock);
    CHECK(result.transition == TransportTransition::started);
    CHECK(result.clock.blockStartBeat == Approx(7.3));
}

TEST_CASE("TransportTracker: stop after start reports stopped", "[TransportTracker]") {
    TransportTracker tracker;
    (void)tracker.update(playing(0.0), sampleRate, oneBeatBlock);
    auto result = tracker.update(stopped(), sampleRate, oneBeatBlock);
    CHECK(result.transition == TransportTransition::stopped);
}

TEST_CASE("TransportTracker: zero or missing tempo is treated as not playing (D-20)", "[TransportTracker]") {
    TransportTracker tracker;
    (void)tracker.update(playing(0.0), sampleRate, oneBeatBlock);
    TransportInfo info = playing(1.0);
    info.tempoBeatsPerMinute = 0.0;
    auto result = tracker.update(info, sampleRate, oneBeatBlock);
    CHECK(result.transition == TransportTransition::stopped);
}

TEST_CASE("TransportTracker: missing musical position is treated as not playing (D-20)", "[TransportTracker]") {
    TransportTracker tracker;
    (void)tracker.update(playing(0.0), sampleRate, oneBeatBlock);
    TransportInfo info = playing(1.0);
    info.hasMusicalPosition = false;
    auto result = tracker.update(info, sampleRate, oneBeatBlock);
    CHECK(result.transition == TransportTransition::stopped);
}

TEST_CASE("TransportTracker: exact continuation reports continuous with expected window start", "[TransportTracker]") {
    TransportTracker tracker;
    (void)tracker.update(playing(0.0), sampleRate, oneBeatBlock);  // expects next block at beat 1.0
    auto result = tracker.update(playing(1.0), sampleRate, oneBeatBlock);
    CHECK(result.transition == TransportTransition::continuous);
    CHECK(result.clock.blockStartBeat == Approx(1.0));
    CHECK(result.clock.windowStartBeat == Approx(1.0));
    CHECK(result.clock.windowEndBeat == Approx(2.0));
}

TEST_CASE("TransportTracker: forward move of at most one buffer is not a discontinuity (REQ-10-12)", "[TransportTracker]") {
    TransportTracker tracker;
    (void)tracker.update(playing(0.0), sampleRate, oneBeatBlock);  // expects next block at beat 1.0
    // Reported playhead is 0.9 beats ahead of the expectation but still within one block's
    // worth of tolerance (<= 1 block forward, sec. 5.4) -> must not be classified discontinuity.
    auto result = tracker.update(playing(1.9), sampleRate, oneBeatBlock);
    CHECK(result.transition != TransportTransition::discontinuity);
}

TEST_CASE("TransportTracker: forward jump beyond one buffer is a discontinuity (REQ-10-7)", "[TransportTracker]") {
    TransportTracker tracker;
    (void)tracker.update(playing(0.0), sampleRate, oneBeatBlock);  // expects next block at beat 1.0
    // 5 beats ahead: far beyond the one-block forward tolerance.
    auto result = tracker.update(playing(5.0), sampleRate, oneBeatBlock);
    CHECK(result.transition == TransportTransition::discontinuity);
}

TEST_CASE("TransportTracker: backward move (loop wrap) is a discontinuity (REQ-10-7)", "[TransportTracker]") {
    TransportTracker tracker;
    (void)tracker.update(playing(8.0), sampleRate, oneBeatBlock);  // expects next block at beat 9.0
    auto result = tracker.update(playing(0.0), sampleRate, oneBeatBlock);  // host loop wrap 8 -> 0
    CHECK(result.transition == TransportTransition::discontinuity);
    CHECK(result.clock.blockStartBeat == Approx(0.0));
    CHECK(result.clock.windowStartBeat == Approx(0.0));
}

TEST_CASE("TransportTracker: locate far forward is a discontinuity", "[TransportTracker]") {
    TransportTracker tracker;
    (void)tracker.update(playing(1.0), sampleRate, 4096);
    auto result = tracker.update(playing(10.0), sampleRate, 4096);
    CHECK(result.transition == TransportTransition::discontinuity);
}

TEST_CASE("TransportTracker: tempo change recomputes samplesPerBeat and keeps schedule continuous", "[TransportTracker]") {
    TransportTracker tracker;
    (void)tracker.update(playing(0.0, 120.0), sampleRate, oneBeatBlock);  // expects next block at beat 1.0
    auto result = tracker.update(playing(1.0, 180.0), sampleRate, oneBeatBlock);
    CHECK(result.transition == TransportTransition::continuous);
    CHECK(result.clock.samplesPerBeat == Approx(16000.0));         // 48000*60/180
    CHECK(result.clock.windowEndBeat == Approx(1.0 + 24000.0 / 16000.0));
}

TEST_CASE("TransportTracker: same-offset piece does not advance the expectation (D-51)", "[TransportTracker]") {
    TransportTracker tracker;
    (void)tracker.update(playing(0.0), sampleRate, oneBeatBlock);  // expects next block at beat 1.0
    // First piece of an oversized same-offset group: advanceExpectation = false.
    auto piece1 = tracker.update(playing(1.0), sampleRate, oneBeatBlock, /*advanceExpectation*/ false);
    CHECK(piece1.transition == TransportTransition::continuous);
    // A second call with the SAME reported playhead must still be continuous (the expectation
    // was not consumed by the first piece), not a discontinuity.
    auto piece2 = tracker.update(playing(1.0), sampleRate, oneBeatBlock, /*advanceExpectation*/ false);
    CHECK(piece2.transition == TransportTransition::continuous);
    CHECK(piece2.clock.windowStartBeat == Approx(1.0));
}

TEST_CASE("TransportTracker: reset forgets history so the next playing block is started again", "[TransportTracker]") {
    TransportTracker tracker;
    (void)tracker.update(playing(0.0), sampleRate, oneBeatBlock);
    tracker.reset();
    auto result = tracker.update(playing(1.0), sampleRate, oneBeatBlock);
    CHECK(result.transition == TransportTransition::started);
}

TEST_CASE("TransportTracker: idle while never started stays idle across calls", "[TransportTracker]") {
    TransportTracker tracker;
    (void)tracker.update(stopped(), sampleRate, oneBeatBlock);
    auto result = tracker.update(stopped(), sampleRate, oneBeatBlock);
    CHECK(result.transition == TransportTransition::idle);
}
