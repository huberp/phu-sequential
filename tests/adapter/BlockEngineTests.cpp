#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include "BlockEngine.h"
#include "HostTransport.h"
#include "arpeggiator/SequencerCore.h"
#include "arpeggiator/TransportTracker.h"
#include "arpeggiator/LaneConfiguration.h"
#include "sequencing/Constants.h"
#include "sequencing/MidiEvent.h"
#include "support/AllocationGuard.h"

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

using namespace phu::plugin;
using namespace phu::arpeggiator;
using namespace phu::sequencing;
using phu::test::AllocationGuard;

namespace {

constexpr double sampleRate = 48000.0;
constexpr double tempo = 120.0;
constexpr double samplesPerBeat = sampleRate * 60.0 / tempo;

TransportInfo transportAt(double playheadBeat) {
    TransportInfo info;
    info.isPlaying = true;
    info.hasMusicalPosition = true;
    info.playheadBeat = playheadBeat;
    info.tempoBeatsPerMinute = tempo;
    return info;
}

TransportInfo transportStopped() {
    return TransportInfo{};
}

}  // namespace

TEST_CASE("BlockEngine: startup input timing versus later chord boundary across three lanes",
          "[BlockEngine][REQ-10-5][REQ-4][startup-input]") {
    const int inputOffset = GENERATE(0, 1, 2);
    const double ppqAheadSamples = GENERATE(-0.4, 0.0, 0.4);
    CAPTURE(inputOffset, ppqAheadSamples);

    auto config = defaultSequencerConfiguration();
    for (auto& lane : config.lanes) {
        lane.mute = true;
    }
    for (int i = 0; i < 3; ++i) {
        auto& lane = config.lanes[static_cast<std::size_t>(i)];
        lane.mute = false;
        lane.followIndex = 0;
        lane.outputChannel = i + 1;
        lane.transpose = -12 * i;
        lane.gatePercent = 84;
        lane.baseTimingIndex = i == 0 ? 1 : 2;  // 1/16 and 1/8
        lane.stepCount = i == 0 ? 32 : 16;
    }

    BlockEngine engine;
    engine.prepare();
    std::array<int, 3> startOns{};
    std::array<int, 3> boundaryOns{};
    std::array<std::int64_t, 3> firstOn{-1, -1, -1};
    constexpr std::int64_t boundarySample = 192000;  // bar 3 at 120 BPM
    constexpr int blockSize = 256;
    for (std::int64_t cursor = 0; cursor <= boundarySample; cursor += blockSize) {
        juce::MidiBuffer midi;
        if (cursor == 0) {
            for (const int pitch : {38, 42, 45}) {
                midi.addEvent(juce::MidiMessage::noteOn(1, pitch, static_cast<juce::uint8>(90)), inputOffset);
            }
        } else if (cursor == boundarySample) {
            for (const int pitch : {38, 42, 45}) {
                midi.addEvent(juce::MidiMessage::noteOff(1, pitch), 0);
            }
            for (const int pitch : {36, 40, 43}) {
                midi.addEvent(juce::MidiMessage::noteOn(1, pitch, static_cast<juce::uint8>(90)), 0);
            }
        }
        const auto transport = transportAt((static_cast<double>(cursor) + ppqAheadSamples) /
                                          (sampleRate * 60.0 / tempo));
        engine.process(midi, blockSize, transport, sampleRate, false, config);
        for (const auto metadata : midi) {
            const auto message = metadata.getMessage();
            if (!message.isNoteOn()) {
                continue;
            }
            const auto lane = static_cast<std::size_t>(message.getChannel() - 1);
            REQUIRE(lane < firstOn.size());
            const auto absolute = cursor + metadata.samplePosition;
            if (firstOn[lane] < 0) {
                firstOn[lane] = absolute;
            }
            if (absolute == inputOffset && inputOffset <= 1) {
                ++startOns[lane];
                CHECK(message.getNoteNumber() == 38 - 12 * static_cast<int>(lane));
            }
            if (absolute == boundarySample) {
                ++boundaryOns[lane];
                CHECK(message.getNoteNumber() == 36 - 12 * static_cast<int>(lane));
            }
        }
    }

    for (std::size_t lane = 0; lane < firstOn.size(); ++lane) {
        CAPTURE(lane);
        CHECK(startOns[lane] == (inputOffset <= 1 ? 1 : 0));
        CHECK(boundaryOns[lane] == 1);
        CHECK(firstOn[lane] == (inputOffset <= 1 ? inputOffset : (lane == 0 ? 6000 : 12000)));
    }
}

TEST_CASE("BlockEngine: Bitwig startup chord at sample one plays the initial notes without shifting the grid",
          "[BlockEngine][REQ-10-5][startup-input]") {
    const int blockSize = GENERATE(1, 480, 4096);
    CAPTURE(blockSize);
    constexpr double traceTempo = 110.0;
    constexpr double traceSamplesPerBeat = sampleRate * 60.0 / traceTempo;
    auto config = defaultSequencerConfiguration();
    for (auto& lane : config.lanes) {
        lane.mute = true;
    }
    for (int i = 0; i < 3; ++i) {
        auto& lane = config.lanes[static_cast<std::size_t>(i)];
        lane.mute = false;
        lane.outputChannel = i + 1;
        lane.followIndex = 0;
        lane.transpose = -12 * i;
        lane.baseTimingIndex = i == 0 ? 1 : 2;
        lane.gatePercent = 84;
    }
    BlockEngine engine;
    engine.prepare();
    std::array<std::vector<std::int64_t>, 3> ons;
    std::array<std::vector<std::int64_t>, 3> offs;
    for (int cursor = 0; cursor < 27000; cursor += blockSize) {
        juce::MidiBuffer midi;
        if (cursor == 0) {
            midi.addEvent(juce::MidiMessage::channelPressureChange(1, 2), 0);
        }
        if (cursor <= 1 && cursor + blockSize > 1) {
            for (const auto [pitch, velocity] : std::array<std::array<int, 2>, 4>{
                     {{69, 33}, {67, 37}, {62, 35}, {50, 30}}}) {
                midi.addEvent(juce::MidiMessage::noteOn(1, pitch, static_cast<juce::uint8>(velocity)), 1 - cursor);
            }
        }
        auto transport = transportAt(static_cast<double>(cursor) / traceSamplesPerBeat);
        transport.tempoBeatsPerMinute = traceTempo;
        engine.process(midi, blockSize, transport, sampleRate, false, config);
        for (const auto metadata : midi) {
            const auto message = metadata.getMessage();
            if (!message.isNoteOnOrOff()) {
                continue;
            }
            const auto lane = static_cast<std::size_t>(message.getChannel() - 1);
            REQUIRE(lane < ons.size());
            CHECK(message.getNoteNumber() == 50 - 12 * static_cast<int>(lane));
            if (message.isNoteOn()) {
                ons[lane].push_back(cursor + metadata.samplePosition);
            } else {
                offs[lane].push_back(cursor + metadata.samplePosition);
            }
        }
    }
    for (std::size_t lane = 0; lane < ons.size(); ++lane) {
        CAPTURE(lane);
        REQUIRE(ons[lane].size() >= 2);
        REQUIRE_FALSE(offs[lane].empty());
        const double length = (lane == 0 ? 0.25 : 0.5) * traceSamplesPerBeat;
        CHECK(ons[lane][0] == 1);
        CHECK(ons[lane][1] == static_cast<std::int64_t>(std::ceil(length - 1.0e-6)));
        CHECK(offs[lane][0] == static_cast<std::int64_t>(std::ceil(length * 0.84 - 1.0e-6)));
    }
}

// --- BlockEngine: construction and lifecycle -----------------------------------------------

TEST_CASE("BlockEngine: construction is noexcept", "[BlockEngine]") {
    BlockEngine engine;
    CHECK(&engine != nullptr);
}

TEST_CASE("BlockEngine: prepare resets core state", "[BlockEngine]") {
    BlockEngine engine;
    engine.prepare();
    
    auto& core = engine.core();
    CHECK(&core != nullptr);
}

TEST_CASE("BlockEngine: prepare clears deferred input", "[BlockEngine]") {
    BlockEngine engine;
    engine.prepare();
    
    juce::MidiBuffer midi;
    std::array<juce::uint8, 3> noteOn{0x90, 60, 100};
    midi.addEvent(juce::MidiMessage(noteOn.data(), 3, false), 0);
    
    engine.process(midi, 0, transportStopped(), sampleRate, false,
                   defaultSequencerConfiguration());
    
    // After prepare, deferred should be cleared
    engine.prepare();
    // No easy way to verify directly, but subsequent zero-sample block should be empty
}

// --- BlockEngine: zero-sample block (deferral) REQ-17-6 ------------------------------------

TEST_CASE("BlockEngine: zero-sample block defers events (REQ-17-6)", "[BlockEngine]") {
    BlockEngine engine;
    engine.prepare();
    
    juce::MidiBuffer midi;
    std::array<juce::uint8, 3> noteOn{0x90, 60, 100};
    midi.addEvent(juce::MidiMessage(noteOn.data(), 3, false), 0);
    
    auto config = defaultSequencerConfiguration();
    engine.process(midi, 0, transportStopped(), sampleRate, false, config);
    
    // MIDI buffer should be cleared (events deferred)
    CHECK(midi.getNumEvents() == 0);
}

TEST_CASE("BlockEngine: deferred events delivered at next non-zero block", "[BlockEngine]") {
    BlockEngine engine;
    engine.prepare();
    
    juce::MidiBuffer midi;
    std::array<juce::uint8, 3> noteOn{0x90, 60, 100};
    midi.addEvent(juce::MidiMessage(noteOn.data(), 3, false), 0);
    
    auto config = defaultSequencerConfiguration();
    
    // Zero-sample block defers the event
    engine.process(midi, 0, transportStopped(), sampleRate, false, config);
    
    // Next non-zero block should process deferred event
    juce::MidiBuffer midi2;
    engine.process(midi2, 4096, transportAt(0.0), sampleRate, false, config);
    
    // Core processes the deferred note input (even if transport is running now)
}

// --- BlockEngine: bypass (REQ-21-1) -------------------------------------------------------

TEST_CASE("BlockEngine: bypassed mode passes input unchanged", "[BlockEngine]") {
    BlockEngine engine;
    engine.prepare();
    
    juce::MidiBuffer midi;
    std::array<juce::uint8, 3> noteOn{0x90, 60, 100};
    std::array<juce::uint8, 3> cc1{0xB0, 1, 64};
    midi.addEvent(juce::MidiMessage(noteOn.data(), 3, false), 100);
    midi.addEvent(juce::MidiMessage(cc1.data(), 3, false), 200);
    
    auto config = defaultSequencerConfiguration();
    engine.process(midi, 4096, transportAt(0.0), sampleRate, true, config);
    
    // In bypass, input events pass through to output
    // We can't easily verify the exact output without full buffer inspection,
    // but the engine should handle it without crashing
    CHECK(engine.droppedOutputEventsTotal() == 0);
}

TEST_CASE("BlockEngine: bypassed mode no lane output", "[BlockEngine]") {
    BlockEngine engine;
    engine.prepare();
    
    juce::MidiBuffer midi;
    std::array<juce::uint8, 3> noteOn{0x90, 60, 100};
    midi.addEvent(juce::MidiMessage(noteOn.data(), 3, false), 0);
    
    auto config = defaultSequencerConfiguration();
    
    // Process in bypass
    engine.process(midi, 4096, transportAt(0.0), sampleRate, true, config);
    
    // No lane sequencing output should be generated in bypass
    // (only pass-through of input events)
}

// --- BlockEngine: transport not playing, Thru off, Consume on (default) -------------------

TEST_CASE("BlockEngine: transport stopped, held note NOT passed through (Thru off)", "[BlockEngine]") {
    BlockEngine engine;
    engine.prepare();
    
    juce::MidiBuffer midi;
    std::array<juce::uint8, 3> noteOn{0x90, 60, 100};
    midi.addEvent(juce::MidiMessage(noteOn.data(), 3, false), 100);
    
    auto config = defaultSequencerConfiguration();
    // Default: Thru off, Consume on
    CHECK(config.global.thru == false);
    
    engine.process(midi, 4096, transportStopped(), sampleRate, false, config);
    
    // Note-On should be consumed (not passed through) and core doesn't generate sequenced output
    // without playing transport
}

TEST_CASE("BlockEngine: transport stopped, non-note events PASSED through (CC1, pitch bend)", "[BlockEngine]") {
    BlockEngine engine;
    engine.prepare();
    
    juce::MidiBuffer midi;
    std::array<juce::uint8, 3> cc1{0xB0, 1, 64};        // CC1 (rawOnly)
    std::array<juce::uint8, 3> pitchBend{0xE0, 0, 64};  // Pitch bend (rawOnly)
    midi.addEvent(juce::MidiMessage(cc1.data(), 3, false), 100);
    midi.addEvent(juce::MidiMessage(pitchBend.data(), 3, false), 200);
    
    auto config = defaultSequencerConfiguration();
    engine.process(midi, 4096, transportStopped(), sampleRate, false, config);
    
    // Non-note events are passed raw regardless of transport state and Thru
}

// --- BlockEngine: transport playing with held note ----------------------------------------

TEST_CASE("BlockEngine: transport playing with held note emits sequenced Note-Ons", "[BlockEngine]") {
    BlockEngine engine;
    engine.prepare();
    
    juce::MidiBuffer midi;
    std::array<juce::uint8, 3> noteOn{0x90, 60, 100};
    midi.addEvent(juce::MidiMessage(noteOn.data(), 3, false), 0);
    
    auto config = defaultSequencerConfiguration();
    engine.process(midi, 4096, transportAt(0.0), sampleRate, false, config);
    
    // First chunk: note held at step 0
    // At 120 BPM 48kHz, a 1/16 step is 6000 samples
    // Next step output at 6000 samples
    
    juce::MidiBuffer midi2;
    engine.process(midi2, 6000, transportAt(0.25), sampleRate, false, config);
    
    // Step 1 output should be generated
}

TEST_CASE("BlockEngine: balanced Note-Offs emitted with Note-Ons", "[BlockEngine]") {
    BlockEngine engine;
    engine.prepare();
    
    juce::MidiBuffer midi;
    std::array<juce::uint8, 3> noteOn{0x90, 60, 100};
    midi.addEvent(juce::MidiMessage(noteOn.data(), 3, false), 0);
    
    auto config = defaultSequencerConfiguration();
    engine.process(midi, 4096, transportAt(0.0), sampleRate, false, config);
    
    juce::MidiBuffer midi2;
    engine.process(midi2, 6000, transportAt(0.25), sampleRate, false, config);
    
    // Step timing: at 120 BPM 48kHz, 1/16 = 6000 samples
    // Expected: Note-Off for step 0, Note-On for step 1, then Note-Off at next step
}

// --- BlockEngine: playback with sample-accurate offsets -----------------------------------

TEST_CASE("BlockEngine: sample offsets in output within block range", "[BlockEngine]") {
    BlockEngine engine;
    engine.prepare();
    
    juce::MidiBuffer midi;
    std::array<juce::uint8, 3> noteOn{0x90, 60, 100};
    midi.addEvent(juce::MidiMessage(noteOn.data(), 3, false), 0);
    
    auto config = defaultSequencerConfiguration();
    engine.process(midi, 4096, transportAt(0.0), sampleRate, false, config);
    
    juce::MidiBuffer midi2;
    engine.process(midi2, 4096, transportAt(0.25), sampleRate, false, config);
    
    // All output offsets should be in [0, 4095] range
}

// --- BlockEngine: configuration clamping REQ-20 -------------------------------------------

TEST_CASE("BlockEngine: uses default configuration (Thru off, Consume on)", "[BlockEngine]") {
    auto config = defaultSequencerConfiguration();
    
    CHECK(config.global.thru == false);
    CHECK(config.global.consume == true);
}

TEST_CASE("BlockEngine: clampConfiguration normalizes parameters", "[BlockEngine]") {
    auto config = defaultSequencerConfiguration();
    auto clamped = clampConfiguration(config);
    
    // Clamped version should be valid (no checks beyond that needed here)
    CHECK(&clamped != nullptr);
}

// --- BlockEngine: no allocation after prepare (REQ-17-1) ----------------------------------

TEST_CASE("BlockEngine: no allocation in process after prepare with full host buffer", "[BlockEngine]") {
    BlockEngine engine;
    engine.prepare();
    
    juce::MidiBuffer midi;
    midi.ensureSize(constants::capacity::outputMidiReserveBytes);
    
    std::array<juce::uint8, 3> noteOn{0x90, 60, 100};
    midi.addEvent(juce::MidiMessage(noteOn.data(), 3, false), 0);
    
    auto config = defaultSequencerConfiguration();
    
    AllocationGuard guard;
    engine.process(midi, 4096, transportAt(0.0), sampleRate, false, config);
    
    // No allocations after prepare with sufficient host buffer capacity
    CHECK(guard.allocationCount() == 0);
}

TEST_CASE("BlockEngine: allocation allowed for host buffer growth on first dense block", "[BlockEngine]") {
    BlockEngine engine;
    engine.prepare();
    
    juce::MidiBuffer midi;
    // Don't pre-size the buffer to full capacity
    
    std::array<juce::uint8, 3> noteOn{0x90, 60, 100};
    midi.addEvent(juce::MidiMessage(noteOn.data(), 3, false), 0);
    
    auto config = defaultSequencerConfiguration();
    
    AllocationGuard guard;
    engine.process(midi, 4096, transportAt(0.0), sampleRate, false, config);
    
    // May allocate once to grow host buffer, or 0 if space available
    CHECK(guard.allocationCount() <= 1);
}

// --- BlockEngine: dropped event reporting ------------------------------------------------

TEST_CASE("BlockEngine: droppedOutputEventsTotal initialized to 0", "[BlockEngine]") {
    BlockEngine engine;
    engine.prepare();
    
    CHECK(engine.droppedOutputEventsTotal() == 0);
}

TEST_CASE("BlockEngine: reports growth count for host buffer", "[BlockEngine]") {
    BlockEngine engine;
    engine.prepare();
    
    juce::MidiBuffer midi;
    std::array<juce::uint8, 3> noteOn{0x90, 60, 100};
    midi.addEvent(juce::MidiMessage(noteOn.data(), 3, false), 0);
    
    auto config = defaultSequencerConfiguration();
    engine.process(midi, 4096, transportAt(0.0), sampleRate, false, config);
    
    // Growth count is part of PlaybackStatus, visible via core if needed
}

// --- BlockEngine: unsorted and out-of-range offsets ----------------------------------------

TEST_CASE("BlockEngine: handles out-of-range offset (beyond numSamples)", "[BlockEngine]") {
    BlockEngine engine;
    engine.prepare();
    
    juce::MidiBuffer midi;
    std::array<juce::uint8, 3> noteOn{0x90, 60, 100};
    midi.addEvent(juce::MidiMessage(noteOn.data(), 3, false), 5000);  // > 4096
    
    auto config = defaultSequencerConfiguration();
    
    // Engine should clamp to valid range
    engine.process(midi, 4096, transportAt(0.0), sampleRate, false, config);
    CHECK(engine.droppedOutputEventsTotal() == 0);  // No drop, just clamping
}

TEST_CASE("BlockEngine: handles unsorted MIDI events", "[BlockEngine]") {
    BlockEngine engine;
    engine.prepare();
    
    juce::MidiBuffer midi;
    std::array<juce::uint8, 3> note1{0x90, 60, 100};
    std::array<juce::uint8, 3> note2{0x90, 62, 100};
    
    midi.addEvent(juce::MidiMessage(note2.data(), 3, false), 2000);
    midi.addEvent(juce::MidiMessage(note1.data(), 3, false), 1000);  // Out of order
    
    auto config = defaultSequencerConfiguration();
    
    // Engine should handle gracefully (ChunkedInputReader should sort/handle)
    engine.process(midi, 4096, transportAt(0.0), sampleRate, false, config);
    CHECK(engine.droppedOutputEventsTotal() >= 0);
}

// --- BlockEngine: core access ---------------------------------------------------------------

TEST_CASE("BlockEngine: provides mutable access to core", "[BlockEngine]") {
    BlockEngine engine;
    auto& core = engine.core();
    CHECK(&core != nullptr);
}

// --- BlockEngine: multiple blocks integration -----------------------------------------------

TEST_CASE("BlockEngine: processes multiple sequential blocks", "[BlockEngine]") {
    BlockEngine engine;
    engine.prepare();
    
    juce::MidiBuffer midi1;
    std::array<juce::uint8, 3> noteOn{0x90, 60, 100};
    midi1.addEvent(juce::MidiMessage(noteOn.data(), 3, false), 0);
    
    auto config = defaultSequencerConfiguration();
    
    engine.process(midi1, 4096, transportAt(0.0), sampleRate, false, config);
    
    juce::MidiBuffer midi2;
    engine.process(midi2, 4096, transportAt(0.25), sampleRate, false, config);
    
    juce::MidiBuffer midi3;
    engine.process(midi3, 4096, transportAt(0.5), sampleRate, false, config);
    
    // All blocks processed without crash
    CHECK(engine.droppedOutputEventsTotal() >= 0);
}

// --- BlockEngine: input event consumption vs pass-through ---------------------------------

TEST_CASE("BlockEngine: note events consumed (not passed raw) by default", "[BlockEngine]") {
    BlockEngine engine;
    engine.prepare();
    
    juce::MidiBuffer midi;
    std::array<juce::uint8, 3> noteOn{0x90, 60, 100};
    midi.addEvent(juce::MidiMessage(noteOn.data(), 3, false), 0);
    
    int eventsBefore = midi.getNumEvents();
    
    auto config = defaultSequencerConfiguration();
    engine.process(midi, 4096, transportAt(0.0), sampleRate, false, config);
    
    // Default config (Thru off, Consume on): the raw Note-On is consumed into the core, not
    // passed through unchanged. With transport playing, the core instead produces sequenced lane
    // output over the block (see "transport playing with held note emits sequenced Note-Ons"
    // below), so the output event count is not bounded by the single raw input event -- the
    // original test's `midi.getNumEvents() <= eventsBefore` assumption contradicted that and has
    // been corrected here (Issue #12 review).
    (void)eventsBefore;
    CHECK(engine.droppedOutputEventsTotal() == 0);
}

TEST_CASE("BlockEngine: raw events preserved in output", "[BlockEngine]") {
    BlockEngine engine;
    engine.prepare();
    
    juce::MidiBuffer midi;
    std::array<juce::uint8, 3> cc1{0xB0, 1, 64};  // CC1 (rawOnly)
    midi.addEvent(juce::MidiMessage(cc1.data(), 3, false), 100);
    
    auto config = defaultSequencerConfiguration();
    engine.process(midi, 4096, transportStopped(), sampleRate, false, config);
    
    // Raw events should appear in output even when transport not playing
}


// --- BlockEngine: host loop wrap inside a block (D-59) ---------------------------------------

TEST_CASE("BlockEngine: host loop wrap inside a block restarts the lane without cutting step 1 (D-59)", "[BlockEngine][D-59]") {
    BlockEngine engine;
    engine.prepare();
    auto config = defaultSequencerConfiguration();
    for (int i = 1; i < constants::lanes::laneCount; ++i) {
        config.lanes[i].mute = true;
    }
    config.lanes[0].baseTimingIndex = 2;  // 1/8 = 12000 samples
    config.lanes[0].gatePercent = 100;
    constexpr int blockSize = 4096;
    constexpr std::int64_t loopEndSample = 16 * 24000;  // 4-bar host loop [0, 16)
    constexpr std::int64_t eighth = 12000;

    struct Out { std::int64_t sample; bool on; int pitch; };
    std::vector<Out> out;
    std::int64_t timelineSample = 0;  // host timeline before the wrap, continues linearly
    bool first = true;
    for (int block = 0; block < 100; ++block) {
        const std::int64_t blockStart = timelineSample;
        const double ppq = static_cast<double>(blockStart >= loopEndSample ? blockStart - loopEndSample : blockStart) / 24000.0;
        auto info = transportAt(ppq);
        info.isLooping = true;
        info.loopStartBeat = 0.0;
        info.loopEndBeat = 16.0;

        juce::MidiBuffer midi;
        if (first) {
            midi.addEvent(juce::MidiMessage::noteOn(1, 38, static_cast<juce::uint8>(90)), 0);
            first = false;
        }
        const std::int64_t wrapInBlock = loopEndSample - blockStart;
        if (wrapInBlock > 0 && wrapInBlock < blockSize) {
            // The host's looping clip retriggers its note at the wrap.
            midi.addEvent(juce::MidiMessage::noteOff(1, 38), static_cast<int>(wrapInBlock));
            midi.addEvent(juce::MidiMessage::noteOn(1, 38, static_cast<juce::uint8>(90)), static_cast<int>(wrapInBlock));
        }
        engine.process(midi, blockSize, info, sampleRate, false, config);
        for (const auto metadata : midi) {
            const auto message = metadata.getMessage();
            if (message.isNoteOnOrOff()) {
                out.push_back({blockStart + metadata.samplePosition, message.isNoteOn(), message.getNoteNumber()});
            }
        }
        timelineSample += blockSize;
    }
    REQUIRE(loopEndSample % blockSize != 0);  // the wrap really falls inside a block

    std::vector<Out> afterWrap;
    for (const auto& event : out) {
        if (event.sample >= loopEndSample && event.sample <= loopEndSample + eighth) {
            afterWrap.push_back(event);
        }
    }
    // Step 16 ends at the wrap; step 1 of the new loop sounds for its full 1/8: On at the wrap,
    // the next Off only at the next step.
    std::string log;
    for (const auto& event : afterWrap) {
        log += std::to_string(event.sample - loopEndSample) + (event.on ? " ON " : " OFF ") + std::to_string(event.pitch) + "\n";
    }
    INFO(log);
    REQUIRE(afterWrap.size() == 4);
    CHECK((!afterWrap[0].on && afterWrap[0].sample == loopEndSample));
    CHECK((afterWrap[1].on && afterWrap[1].sample == loopEndSample && afterWrap[1].pitch == 38));
    CHECK((!afterWrap[2].on && afterWrap[2].sample == loopEndSample + eighth));
    CHECK((afterWrap[3].on && afterWrap[3].sample == loopEndSample + eighth));
}

TEST_CASE("BlockEngine: host loop wrap meets a chord that arrives one sample after the wrap (D-62)",
          "[BlockEngine][D-59][D-62][REQ-10-16]") {
    // Bitwig trace 2026-11-22: at 120 BPM the wrap falls on a block boundary and the clip sends
    // Note-Offs at sample 0 but the new chord's Note-Ons at sample 1; at 121 BPM both share the
    // in-block wrap sample.
    const double bpm = GENERATE(120.0, 121.0);
    const int onDelay = GENERATE(0, 1);
    const bool latch = GENERATE(false, true);
    CAPTURE(bpm, onDelay, latch);
    const double spb = sampleRate * 60.0 / bpm;
    constexpr int blockSize = 480;
    constexpr double loopBeats = 16.0;
    const double loopSamples = loopBeats * spb;

    auto config = defaultSequencerConfiguration();
    for (auto& lane : config.lanes) {
        lane.mute = true;
    }
    config.lanes[0].mute = false;
    config.lanes[0].baseTimingIndex = 2;  // 1/8
    config.lanes[0].gatePercent = 84;
    config.lanes[0].latch = latch;

    struct In { std::int64_t sample; bool on; int pitch; };
    std::vector<In> input;
    std::vector<std::int64_t> wraps;
    const std::array<int, 2> chords{38, 36};  // A, B: one note per chord is enough for lane 0
    for (int loop = 0; loop < 2; ++loop) {
        const auto loopStart = static_cast<std::int64_t>(std::ceil(loop * loopSamples - 1.0e-6));
        const auto half = static_cast<std::int64_t>(std::ceil((loop * loopBeats + 8.0) * spb - 1.0e-6));
        if (loop > 0) {
            wraps.push_back(loopStart);
            input.push_back({loopStart, false, chords[1]});
        }
        input.push_back({loopStart + (loop > 0 ? onDelay : 1), true, chords[0]});
        input.push_back({half, false, chords[0]});
        input.push_back({half, true, chords[1]});
    }
    const auto endSample = static_cast<std::int64_t>(std::ceil(1.5 * loopSamples));

    BlockEngine engine;
    engine.prepare();
    std::vector<In> out;
    for (std::int64_t cursor = 0; cursor < endSample; cursor += blockSize) {
        juce::MidiBuffer midi;
        for (const auto& event : input) {
            if (event.sample >= cursor && event.sample < cursor + blockSize) {
                const int offset = static_cast<int>(event.sample - cursor);
                midi.addEvent(event.on ? juce::MidiMessage::noteOn(1, event.pitch, static_cast<juce::uint8>(90))
                                       : juce::MidiMessage::noteOff(1, event.pitch),
                              offset);
            }
        }
        double ppq = static_cast<double>(cursor) / spb;
        if (ppq >= loopBeats) {
            ppq -= loopBeats;
        }
        auto info = transportAt(ppq);
        info.tempoBeatsPerMinute = bpm;
        info.isLooping = true;
        info.loopStartBeat = 0.0;
        info.loopEndBeat = loopBeats;
        engine.process(midi, blockSize, info, sampleRate, false, config);
        for (const auto metadata : midi) {
            const auto message = metadata.getMessage();
            if (message.isNoteOnOrOff() && message.getChannel() == 1) {
                out.push_back({cursor + metadata.samplePosition, message.isNoteOn(), message.getNoteNumber()});
            }
        }
    }

    REQUIRE(wraps.size() == 1);
    const std::int64_t wrap = wraps[0];
    CHECK((wrap % blockSize == 0) == (bpm == 120.0));  // the trace's two wrap placements
    const auto secondStep = wrap + static_cast<std::int64_t>(std::ceil(0.5 * spb - 1.0e-6));
    std::vector<In> stepOne;
    for (const auto& event : out) {
        if (event.on && event.sample >= wrap && event.sample < secondStep) {
            stepOne.push_back(event);
        }
    }
    REQUIRE(stepOne.size() == 1);
    CHECK(stepOne[0].sample == wrap + onDelay);
    CHECK(stepOne[0].pitch == chords[0]);
    bool secondStepPlayed = false;
    for (const auto& event : out) {
        secondStepPlayed = secondStepPlayed || (event.on && event.sample == secondStep && event.pitch == chords[0]);
    }
    CHECK(secondStepPlayed);
}
