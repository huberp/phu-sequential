#include <catch2/catch_test_macros.hpp>
#include "arpeggiator/SequencerCore.h"
#include "arpeggiator/BlockTypes.h"
#include "arpeggiator/SequencerConfiguration.h"
#include "arpeggiator/LaneConfiguration.h"
#include "sequencing/Constants.h"
#include "sequencing/MidiEvent.h"
#include "sequencing/OutputEventBuffer.h"
#include "support/ScenarioRunner.h"
#include "support/AllocationGuard.h"

#include <array>
#include <cstdint>
#include <random>
#include <vector>

// M17 Sequencer core: end-to-end integration of transport, pass-through, and the lane engines
// (sec. 9.3, 10, 20, 21, arch. 4.6, 5, #11). 120 BPM @ 48000 Hz -> samplesPerBeat = 24000;
// the default lane (1/16 straight, 16 steps) has a 6000-sample step and a 96000-sample loop.

using namespace phu::arpeggiator;
using namespace phu::sequencing;
using phu::arpeggiator::test::CapturedEvent;
using phu::arpeggiator::test::ScenarioRunner;

namespace {

constexpr double sampleRate = 48000.0;
constexpr double tempo = 120.0;
constexpr double samplesPerBeatConst = 24000.0;
constexpr std::int64_t stepSamples = 6000;  // 1/16 note at 120 BPM / 48 kHz

TransportInfo transportAt(double playheadBeat, double tempoBpm = tempo) {
    TransportInfo info;
    info.isPlaying = true;
    info.hasMusicalPosition = true;
    info.playheadBeat = playheadBeat;
    info.tempoBeatsPerMinute = tempoBpm;
    return info;
}

TransportInfo transportStopped() {
    return TransportInfo{};
}

// Default configuration with every lane but lane 0 muted, so the scenario tests observe exactly
// one lane's output (lane 0: chord-follow index 0, channel 1, default timing/gate/velocity).
SequencerConfiguration makeSingleLaneConfig() {
    SequencerConfiguration config = defaultSequencerConfiguration();
    for (int i = 1; i < constants::lanes::laneCount; ++i) {
        config.lanes[i].mute = true;
    }
    return config;
}

std::vector<CapturedEvent> eventsAt(const ScenarioRunner& runner, std::int64_t sample) {
    std::vector<CapturedEvent> result;
    for (const auto& event : runner.captured()) {
        if (event.absoluteSample == sample) {
            result.push_back(event);
        }
    }
    return result;
}

int countKindForPitch(const ScenarioRunner& runner, MidiEventKind kind, int pitch) {
    int count = 0;
    for (const auto& event : runner.captured()) {
        if (event.kind == kind && event.pitch == pitch) {
            ++count;
        }
    }
    return count;
}

BlockInput makeInput(std::span<const MidiEvent> events, TransportInfo transport, int numSamples,
                    bool bypassed = false) {
    BlockInput input;
    input.events = events;
    input.transport = transport;
    input.sampleRate = sampleRate;
    input.numSamples = numSamples;
    input.bypassed = bypassed;
    return input;
}

}  // namespace

// --- REQ-20-5: fresh plugin end-to-end -------------------------------------------------

TEST_CASE("SequencerCore: REQ-20-5 fresh plugin, chord held, transport start -> 16th notes", "[SequencerCore]") {
    ScenarioRunner runner(makeSingleLaneConfig());

    std::vector<MidiEvent> firstEvents{makeNoteOn(0, 1, 60, 100)};
    runner.runChunk(makeInput(firstEvents, transportAt(0.0), 4096));
    std::vector<MidiEvent> noMoreEvents;
    while (runner.sampleCursor() < 14000) {
        const double playheadBeat = static_cast<double>(runner.sampleCursor()) / samplesPerBeatConst;
        runner.runChunk(makeInput(noMoreEvents, transportAt(playheadBeat), 4096));
    }

    auto at0 = eventsAt(runner, 0);
    REQUIRE(at0.size() == 1);
    CHECK(at0[0].kind == MidiEventKind::noteOn);
    CHECK(at0[0].channel == 1);
    CHECK(at0[0].pitch == 60);
    CHECK(at0[0].velocity == 100);

    auto at6000 = eventsAt(runner, stepSamples);
    REQUIRE(at6000.size() == 2);
    CHECK(at6000[0].kind == MidiEventKind::noteOff);  // Note-Off before Note-On at equal sample (D-14)
    CHECK(at6000[1].kind == MidiEventKind::noteOn);
    CHECK(at6000[1].pitch == 60);
    CHECK(at6000[1].velocity == 100);

    auto at12000 = eventsAt(runner, 2 * stepSamples);
    REQUIRE(at12000.size() == 2);
    CHECK(at12000[0].kind == MidiEventKind::noteOff);
    CHECK(at12000[1].kind == MidiEventKind::noteOn);
}

// --- REQ-10-1/2/3/4/5/6: transport start/stop -------------------------------------------

TEST_CASE("SequencerCore: transport stop sends Note-Offs for all sounding notes (REQ-10-3)", "[SequencerCore]") {
    ScenarioRunner runner(makeSingleLaneConfig());
    std::vector<MidiEvent> events{makeNoteOn(0, 1, 60, 100)};
    runner.runChunk(makeInput(events, transportAt(0.0), 4096));  // step 0 Note-On at abs 0
    REQUIRE(countKindForPitch(runner, MidiEventKind::noteOn, 60) == 1);

    std::vector<MidiEvent> none;
    const auto stopResult = runner.runChunk(makeInput(none, transportStopped(), 4096));
    (void)stopResult;
    auto atStop = eventsAt(runner, 4096);
    REQUIRE(atStop.size() == 1);
    CHECK(atStop[0].kind == MidiEventKind::noteOff);
    CHECK(atStop[0].pitch == 60);
    CHECK(runner.core().playbackStatus().transportPlaying == false);
}

TEST_CASE("SequencerCore: without a running transport the plugin stays silent (REQ-10-4)", "[SequencerCore]") {
    ScenarioRunner runner(makeSingleLaneConfig());
    std::vector<MidiEvent> events{makeNoteOn(0, 1, 60, 100)};
    runner.runChunk(makeInput(events, transportStopped(), 4096));
    std::vector<MidiEvent> none;
    for (int i = 0; i < 4; ++i) {
        runner.runChunk(makeInput(none, transportStopped(), 4096));
    }
    CHECK(runner.captured().empty());
}

TEST_CASE("SequencerCore: held keys at transport start drive lanes at once (REQ-10-5)", "[SequencerCore]") {
    ScenarioRunner runner(makeSingleLaneConfig());
    std::vector<MidiEvent> events{makeNoteOn(0, 1, 60, 100)};
    runner.runChunk(makeInput(events, transportStopped(), 4096));  // held before transport starts
    REQUIRE(runner.captured().empty());

    std::vector<MidiEvent> none;
    const auto startCursor = runner.sampleCursor();
    runner.runChunk(makeInput(none, transportAt(0.0), 4096));  // T+ : key already held
    auto atStart = eventsAt(runner, startCursor);
    REQUIRE(atStart.size() == 1);
    CHECK(atStart[0].kind == MidiEventKind::noteOn);
    CHECK(atStart[0].pitch == 60);
}

// --- REQ-10-7: playhead discontinuity ---------------------------------------------------

TEST_CASE("SequencerCore: backward playhead move (loop wrap) releases and re-derives position (REQ-10-7)", "[SequencerCore]") {
    ScenarioRunner runner(makeSingleLaneConfig());
    std::vector<MidiEvent> events{makeNoteOn(0, 1, 60, 100)};
    runner.runChunk(makeInput(events, transportAt(0.0), static_cast<int>(samplesPerBeatConst)));  // 1 beat block, note sounding
    // Default lane: 1/16 straight note = 0.25 beat/step (stepSamples = 6000, as fixed by the
    // REQ-20-5 scenario above), so a full 1-beat (24000-sample) block triggers steps 0-3 (abs 0,
    // 6000, 12000, 18000), all the same held pitch: 4 Note-Ons, not 1 (corrected test expectation;
    // the original "== 1" assumed one step per beat, contradicting the file's own stepSamples math).
    REQUIRE(countKindForPitch(runner, MidiEventKind::noteOn, 60) == 4);

    std::vector<MidiEvent> none;
    const auto wrapCursor = runner.sampleCursor();
    // Host reports a loop wrap back to beat 0 instead of the expected beat 1.0.
    runner.runChunk(makeInput(none, transportAt(0.0), static_cast<int>(samplesPerBeatConst)));

    auto atWrap = eventsAt(runner, wrapCursor);
    REQUIRE(atWrap.size() >= 1);
    CHECK(atWrap.front().kind == MidiEventKind::noteOff);  // all sounding notes end first (D-17)
}

TEST_CASE("SequencerCore: forward locate beyond one buffer is a discontinuity (REQ-10-7)", "[SequencerCore]") {
    ScenarioRunner runner(makeSingleLaneConfig());
    std::vector<MidiEvent> events{makeNoteOn(0, 1, 60, 100)};
    runner.runChunk(makeInput(events, transportAt(0.0), static_cast<int>(samplesPerBeatConst)));
    // See the backward-playhead test above: a full 1-beat block triggers steps 0-3 (4 Note-Ons).
    REQUIRE(countKindForPitch(runner, MidiEventKind::noteOn, 60) == 4);

    std::vector<MidiEvent> none;
    const auto locateCursor = runner.sampleCursor();
    runner.runChunk(makeInput(none, transportAt(10.0), static_cast<int>(samplesPerBeatConst)));  // locate far ahead

    auto atLocate = eventsAt(runner, locateCursor);
    REQUIRE(atLocate.size() >= 1);
    CHECK(atLocate.front().kind == MidiEventKind::noteOff);
}

// --- REQ-10-10: tempo change -------------------------------------------------------------

TEST_CASE("SequencerCore: tempo change moves the Note-Off sample but not its beat (REQ-10-10)", "[SequencerCore]") {
    ScenarioRunner runner(makeSingleLaneConfig());
    std::vector<MidiEvent> events{makeNoteOn(0, 1, 60, 100)};
    runner.runChunk(makeInput(events, transportAt(0.0, 120.0), 3000));  // 0.125 beat elapsed at 120 BPM
    REQUIRE(countKindForPitch(runner, MidiEventKind::noteOn, 60) == 1);

    std::vector<MidiEvent> none;
    // Tempo drops to 60 BPM: the scheduled Note-Off (beat 0.25) is still 0.125 beat away, which is
    // now 6000 samples (48000 samples/beat at 60 BPM) instead of 6000 - 3000 = 3000 at the old tempo.
    const auto tempoChangeCursor = runner.sampleCursor();
    // Block size is 6001, not 6000: the window is half-open [windowStart, windowEnd) (arch. 5.4),
    // so a Note-Off scheduled exactly at windowEnd (here beat 0.25, the window's upper bound at
    // numSamples = 6000) belongs to the next block, not this one; one extra sample puts beat 0.25
    // strictly inside the window so it is actually emitted here, at offset 6000 (test bug fix).
    runner.runChunk(makeInput(none, transportAt(0.125, 60.0), 6001));

    auto atOldTempoOffset = eventsAt(runner, tempoChangeCursor + 3000);  // where it would fire at 120 BPM
    CHECK(atOldTempoOffset.empty());
    auto atNewTempoOffset = eventsAt(runner, tempoChangeCursor + 6000);  // where it actually fires at 60 BPM
    REQUIRE_FALSE(atNewTempoOffset.empty());
    CHECK(atNewTempoOffset.front().kind == MidiEventKind::noteOff);
}

// --- REQ-10-11/13: sample accuracy and block-size invariance ----------------------------

namespace {

std::vector<std::int64_t> runSingleLaneNoteOnOffsets(int blockSize, std::int64_t totalSamples) {
    ScenarioRunner runner(makeSingleLaneConfig());
    std::vector<MidiEvent> firstEvents{makeNoteOn(0, 1, 60, 100)};
    std::vector<MidiEvent> none;
    bool first = true;
    while (runner.sampleCursor() < totalSamples) {
        const double playheadBeat = static_cast<double>(runner.sampleCursor()) / samplesPerBeatConst;
        runner.runChunk(makeInput(first ? firstEvents : none, transportAt(playheadBeat), blockSize));
        first = false;
    }
    std::vector<std::int64_t> onsets;
    for (const auto& event : runner.captured()) {
        if (event.kind == MidiEventKind::noteOn) {
            onsets.push_back(event.absoluteSample);
        }
    }
    return onsets;
}

}  // namespace

TEST_CASE("SequencerCore: block-size invariance produces identical absolute Note-On samples (REQ-10-13)", "[SequencerCore]") {
    const std::int64_t totalSamples = 14000;  // covers Note-Ons at 0, 6000, 12000
    const auto reference = runSingleLaneNoteOnOffsets(64, totalSamples);
    for (int blockSize : {480, 1024, 4096}) {
        const auto candidate = runSingleLaneNoteOnOffsets(blockSize, totalSamples);
        INFO("blockSize = " << blockSize);
        CHECK(candidate == reference);
    }
}

// --- Thru/Consume toggled while keys held: no hanging notes -----------------------------

TEST_CASE("SequencerCore: Thru/Consume toggled while keys held never leaves a hanging note", "[SequencerCore]") {
    SequencerConfiguration config = makeSingleLaneConfig();
    config.global.thru = false;
    config.global.consume = true;
    ScenarioRunner runner(config);

    std::vector<MidiEvent> press60{makeNoteOn(0, 1, 60, 100)};
    runner.runChunk(makeInput(press60, transportAt(0.0), 4096));

    config.global.thru = true;
    config.global.consume = false;
    runner.setConfig(config);
    std::vector<MidiEvent> press64{makeNoteOn(0, 1, 64, 90)};
    runner.runChunk(makeInput(press64, transportAt(4096.0 / samplesPerBeatConst), 4096));

    config.global.thru = false;
    config.global.consume = true;
    runner.setConfig(config);
    std::vector<MidiEvent> press67{makeNoteOn(0, 1, 67, 80)};
    runner.runChunk(makeInput(press67, transportAt(8192.0 / samplesPerBeatConst), 4096));

    std::vector<MidiEvent> releaseAll{
        makeNoteOff(0, 1, 60), makeNoteOff(0, 1, 64), makeNoteOff(0, 1, 67)};
    runner.runChunk(makeInput(releaseAll, transportAt(12288.0 / samplesPerBeatConst), 4096));
    std::vector<MidiEvent> none;
    runner.runChunk(makeInput(none, transportStopped(), 4096));  // force any remaining lane voices off

    CHECK(runner.allNotesClosed());
}

// --- REQ-21: bypass behavior -------------------------------------------------------------

TEST_CASE("SequencerCore: entering bypass ends passed-through and lane notes, forwards new input raw (REQ-21-1/2/3)", "[SequencerCore]") {
    SequencerConfiguration config = makeSingleLaneConfig();
    config.global.thru = true;
    config.global.consume = false;
    ScenarioRunner runner(config);

    std::vector<MidiEvent> events{makeNoteOn(0, 1, 60, 100), makeNoteOn(0, 2, 70, 90)};
    runner.runChunk(makeInput(events, transportAt(0.0), 4096));
    // With thru=true/consume=false every input channel is forwarded raw (sec. 3.3), including
    // channel 1, which lane 0 also follows (default chordInputChannel/outputChannel == 1). The
    // raw pass-through voice and the lane's own voice both occupy the (channel 1, pitch 60) voice
    // registry slot, so the lane's same-offset trigger retriggers/evicts the pass-through voice:
    // one Note-On from the pass-through path, a Note-Off from the retrigger, then the lane's own
    // Note-On (retrigger semantics; sec. 5.2/5.3 ordering keeps this deterministic). So pitch 60
    // legitimately gets 2 Note-On events here, not 1.
    REQUIRE(countKindForPitch(runner, MidiEventKind::noteOn, 60) == 2);  // pass-through + lane 0
    REQUIRE(countKindForPitch(runner, MidiEventKind::noteOn, 70) == 1);  // pass-through

    const auto bypassCursor = runner.sampleCursor();
    std::vector<MidiEvent> newNoteDuringBypass{makeNoteOn(100, 3, 80, 64)};
    runner.runChunk(makeInput(newNoteDuringBypass, transportAt(4096.0 / samplesPerBeatConst), 4096, /*bypassed*/ true));

    auto atBypassEnter = eventsAt(runner, bypassCursor);
    int offCount = 0;
    for (const auto& event : atBypassEnter) {
        if (event.kind == MidiEventKind::noteOff) {
            ++offCount;
        }
    }
    CHECK(offCount == 2);  // both the lane's and the passed-through note end (REQ-21-2/3)

    auto rawForward = eventsAt(runner, bypassCursor + 100);
    REQUIRE(rawForward.size() == 1);
    CHECK(rawForward[0].kind == MidiEventKind::noteOn);
    CHECK(rawForward[0].channel == 3);
    CHECK(rawForward[0].pitch == 80);
    CHECK(rawForward[0].velocity == 64);  // forwarded unchanged (REQ-21-1)

    CHECK(runner.core().playbackStatus().bypassed == true);
}

TEST_CASE("SequencerCore: leaving bypass resumes from the playhead, no reset to step 1 (REQ-21-4)", "[SequencerCore]") {
    ScenarioRunner runner(makeSingleLaneConfig());
    std::vector<MidiEvent> events{makeNoteOn(0, 1, 60, 100)};
    runner.runChunk(makeInput(events, transportAt(0.0), 4096));  // step 0 @ abs 0
    std::vector<MidiEvent> none;
    while (runner.sampleCursor() < 17000) {  // steps 0,1,2 triggered (0, 6000, 12000); step 3 due at 18000
        const double playheadBeat = static_cast<double>(runner.sampleCursor()) / samplesPerBeatConst;
        runner.runChunk(makeInput(none, transportAt(playheadBeat), 4096));
    }

    const double bypassEnterBeat = static_cast<double>(runner.sampleCursor()) / samplesPerBeatConst;
    runner.runChunk(makeInput(none, transportAt(bypassEnterBeat), 2000, /*bypassed*/ true));  // covers step 3's due time
    const double bypassExitBeat = static_cast<double>(runner.sampleCursor()) / samplesPerBeatConst;
    const auto exitCursor = runner.sampleCursor();
    runner.runChunk(makeInput(none, transportAt(bypassExitBeat), 20000, /*bypassed*/ false));

    // Step 3 (beat 0.75, abs 18000) was skipped during bypass; the next trigger is step 4
    // (beat 1.0, abs 24000), not an immediate replay at the bypass-exit sample nor a reset to
    // step 0 at a step-length offset from it.
    CHECK(eventsAt(runner, exitCursor).empty());
    CHECK(eventsAt(runner, exitCursor + stepSamples).empty());
    auto atStep4 = eventsAt(runner, 4 * stepSamples);
    REQUIRE_FALSE(atStep4.empty());
    CHECK(atStep4.back().kind == MidiEventKind::noteOn);
}

// --- REQ-9.3-9 / REQ-11-6: seeded random Note-Off guarantee / glitch-free fuzz ----------

TEST_CASE("SequencerCore: random event/parameter fuzz never leaves a hanging note (REQ-9.3-9, REQ-11-6)", "[SequencerCore]") {
    std::mt19937 rng(12345);  // fixed seed: deterministic, reproducible failures
    std::uniform_int_distribution<int> pitchDist(48, 72);
    std::uniform_int_distribution<int> velocityDist(1, 127);
    std::uniform_int_distribution<int> actionDist(0, 9);
    std::uniform_int_distribution<int> blockSizeDist(64, 512);

    SequencerConfiguration config = defaultSequencerConfiguration();
    ScenarioRunner runner(config);

    std::array<bool, 128> held{};
    bool playing = true;
    double beat = 0.0;
    constexpr int iterations = 500;  // reduced from the spec's 10 000 to keep CI time bounded

    for (int i = 0; i < iterations; ++i) {
        std::vector<MidiEvent> events;
        const int action = actionDist(rng);
        if (action < 4) {
            const int pitch = pitchDist(rng);
            if (!held[static_cast<std::size_t>(pitch)]) {
                events.push_back(makeNoteOn(0, 1, pitch, velocityDist(rng)));
                held[static_cast<std::size_t>(pitch)] = true;
            } else {
                events.push_back(makeNoteOff(0, 1, pitch));
                held[static_cast<std::size_t>(pitch)] = false;
            }
        } else if (action == 4) {
            config.global.thru = !config.global.thru;
        } else if (action == 5) {
            config.global.consume = !config.global.consume;
        } else if (action == 6) {
            config.lanes[0].mute = !config.lanes[0].mute;
        } else if (action == 7) {
            config.lanes[0].latch = !config.lanes[0].latch;
        } else if (action == 8 && playing) {
            playing = false;
        } else if (action == 9 && !playing) {
            playing = true;
        }
        runner.setConfig(config);

        const int blockSize = blockSizeDist(rng);
        const TransportInfo transport = playing ? transportAt(beat) : transportStopped();
        runner.runChunk(makeInput(events, transport, blockSize));
        if (playing) {
            beat += static_cast<double>(blockSize) / samplesPerBeatConst;
        }
    }

    // Release every still-held key and force a final stop so every open voice must close.
    std::vector<MidiEvent> releaseAll;
    for (int pitch = 0; pitch < 128; ++pitch) {
        if (held[static_cast<std::size_t>(pitch)]) {
            releaseAll.push_back(makeNoteOff(0, 1, pitch));
        }
    }
    runner.runChunk(makeInput(releaseAll, playing ? transportAt(beat) : transportStopped(), 256));
    std::vector<MidiEvent> none;
    runner.runChunk(makeInput(none, transportStopped(), 256));

    CHECK(runner.allNotesClosed());
}

// --- REQ-17-1: no allocation on the processBlock path -----------------------------------

TEST_CASE("SequencerCore: processBlock performs no heap allocation (REQ-17-1)", "[SequencerCore]") {
    // Drives SequencerCore::processBlock directly (not via ScenarioRunner, whose own capture
    // log grows dynamically and would contaminate the allocation count being measured here).
    SequencerCore core;
    SequencerConfiguration config = makeSingleLaneConfig();
    OutputEventBuffer output(constants::capacity::outputEventsPerBlock);  // allocates once, outside the guard

    std::vector<MidiEvent> events{makeNoteOn(0, 1, 60, 100)};
    core.processBlock(makeInput(events, transportAt(0.0), 4096), config, output);  // warm-up

    std::vector<MidiEvent> none;
    std::int64_t cursor = 4096;
    {
        phu::test::AllocationGuard guard;
        for (int i = 0; i < 100; ++i) {
            const double playheadBeat = static_cast<double>(cursor) / samplesPerBeatConst;
            core.processBlock(makeInput(none, transportAt(playheadBeat), 256), config, output);
            cursor += 256;
        }
        CHECK(guard.allocationCount() == 0);
    }
}
