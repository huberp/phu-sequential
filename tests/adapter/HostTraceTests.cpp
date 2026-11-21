#include <catch2/catch_test_macros.hpp>

#include "BlockEngine.h"
#include "HostTrace.h"
#include "ParameterLayout.h"
#include "ParameterReader.h"
#include "PatternStore.h"
#include "StateSerializer.h"
#include "sequencing/Constants.h"
#include "support/AllocationGuard.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <cmath>
#include <cstdint>
#include <sstream>
#include <string>
#include <vector>

// M28 (D-61): host trace recorder, text format, file writer and deterministic replay.

using namespace phu::plugin;
using namespace phu::arpeggiator;

namespace {

struct DummyProcessor : juce::AudioProcessor {
    DummyProcessor() : juce::AudioProcessor(BusesProperties()) {}
    const juce::String getName() const override { return "Dummy"; }
    void prepareToPlay(double, int) override {}
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
    double getTailLengthSeconds() const override { return 0.0; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override {}
    void setStateInformation(const void*, int) override {}
};

// The parts of PluginProcessor that processInternal uses, without the JUCE plugin wrapper.
struct PluginRig {
    DummyProcessor processor;
    juce::AudioProcessorValueTreeState parameters{processor, nullptr, "Parameters", createParameterLayout()};
    ParameterReader reader{parameters};
    PatternStore patterns;
    BlockEngine engine;

    PluginRig() { engine.prepare(); }

    void captureStateBlock(juce::MemoryBlock& out) { writeStateBlock(captureState(parameters, patterns), out); }

    void loadStateBlock(const juce::MemoryBlock& state) {
        const auto loaded = readStateBlock(state.getData(), static_cast<int>(state.getSize()));
        applyState(loaded.data, parameters, patterns);
    }

    void process(juce::MidiBuffer& midi, int numSamples, const TransportInfo& transport, double sampleRate) {
        processPluginBlock(engine, reader, patterns.audioSide(), midi, numSamples, transport, sampleRate, false);
    }
};

std::vector<HostTraceMidi> toTraceMidi(const juce::MidiBuffer& midi) {
    std::vector<HostTraceMidi> out;
    for (const auto metadata : midi) {
        out.push_back({metadata.samplePosition, std::vector<std::uint8_t>(metadata.data, metadata.data + metadata.numBytes)});
    }
    return out;
}

// Replays every block of a parsed trace through a fresh rig; returns the plugin output per block.
std::vector<std::vector<HostTraceMidi>> replay(const HostTraceParseResult& trace) {
    PluginRig rig;
    std::vector<std::vector<HostTraceMidi>> outputs;
    for (const auto& block : trace.blocks) {
        if (block.stateBefore.has_value()) {
            rig.loadStateBlock(*block.stateBefore);
        }
        juce::MidiBuffer midi;
        for (const auto& event : block.input) {
            midi.addEvent(event.bytes.data(), static_cast<int>(event.bytes.size()), event.offset);
        }
        processPluginBlock(rig.engine, rig.reader, rig.patterns.audioSide(), midi, block.numSamples, block.transport,
                           block.sampleRate, block.bypassed);
        outputs.push_back(toTraceMidi(midi));
    }
    return outputs;
}

TransportInfo playingAt(double ppq) {
    TransportInfo info;
    info.isPlaying = true;
    info.hasMusicalPosition = true;
    info.playheadBeat = ppq;
    info.tempoBeatsPerMinute = 120.0;
    return info;
}

std::string readFile(const juce::File& file) {
    return file.loadFileAsString().toStdString();
}

}  // namespace

TEST_CASE("HostTrace: recorder publishes complete blocks in order", "[HostTrace][M28]") {
    HostTraceRecorder recorder(64);
    juce::MidiBuffer input;
    input.addEvent(juce::MidiMessage::noteOn(1, 60, static_cast<juce::uint8>(100)), 5);
    juce::MidiBuffer output;
    output.addEvent(juce::MidiMessage::noteOff(2, 61), 7);

    recorder.beginBlock(512, 48000.0, false, playingAt(1.5));
    recorder.addMidi(HostTraceRecord::Kind::input, input);
    std::vector<HostTraceRecord> drained;
    CHECK(recorder.drain(drained) == 0);  // block still open
    recorder.addMidi(HostTraceRecord::Kind::output, output);
    recorder.endBlock();

    REQUIRE(recorder.drain(drained) == 3);
    CHECK(drained[0].kind == HostTraceRecord::Kind::block);
    CHECK(drained[0].value == 512);
    CHECK(drained[0].transport.playheadBeat == 1.5);
    CHECK(drained[1].kind == HostTraceRecord::Kind::input);
    CHECK(drained[1].value == 5);
    CHECK(drained[1].size == 3);
    CHECK(drained[1].bytes[1] == 60);
    CHECK(drained[2].kind == HostTraceRecord::Kind::output);
    CHECK(drained[2].bytes[0] == 0x81);
}

TEST_CASE("HostTrace: a block that does not fit is dropped as a whole and counted", "[HostTrace][M28]") {
    HostTraceRecorder recorder(4);
    juce::MidiBuffer three;
    for (int i = 0; i < 3; ++i) {
        three.addEvent(juce::MidiMessage::noteOn(1, 60 + i, static_cast<juce::uint8>(100)), i);
    }
    recorder.beginBlock(64, 48000.0, false, playingAt(0.0));
    recorder.addMidi(HostTraceRecord::Kind::input, three);
    recorder.endBlock();  // 4 records: fits exactly
    recorder.beginBlock(64, 48000.0, false, playingAt(0.1));
    recorder.endBlock();  // ring full: dropped
    CHECK(recorder.droppedBlocks() == 1);

    std::vector<HostTraceRecord> drained;
    CHECK(recorder.drain(drained) == 4);
    recorder.beginBlock(64, 48000.0, false, playingAt(0.2));
    recorder.endBlock();  // space again after the drain
    drained.clear();
    REQUIRE(recorder.drain(drained) == 1);
    CHECK(drained[0].transport.playheadBeat == 0.2);
    CHECK(recorder.droppedBlocks() == 1);
}

TEST_CASE("HostTrace: recording a block does not allocate", "[HostTrace][M28][REQ-17-1]") {
    HostTraceRecorder recorder(256);
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, static_cast<juce::uint8>(100)), 0);
    phu::test::AllocationGuard guard;
    recorder.beginBlock(128, 44100.0, true, playingAt(3.0));
    recorder.addMidi(HostTraceRecord::Kind::input, midi);
    recorder.addMidi(HostTraceRecord::Kind::output, midi);
    recorder.endBlock();
    CHECK(guard.allocationCount() == 0);
}

TEST_CASE("HostTrace: text format round-trips block and MIDI records exactly", "[HostTrace][M28]") {
    HostTraceRecord block;
    block.value = 441;
    block.sampleRate = 44100.0;
    block.bypassed = true;
    block.transport = playingAt(0.1 + 0.2);  // not exactly representable in decimal
    block.transport.tempoBeatsPerMinute = 133.33333333333334;
    block.transport.isLooping = true;
    block.transport.loopStartBeat = 4.0;
    block.transport.loopEndBeat = 20.000000000000004;
    HostTraceRecord input;
    input.kind = HostTraceRecord::Kind::input;
    input.value = 17;
    input.size = 3;
    input.bytes = {0x90, 0x3c, 0x64};
    HostTraceRecord output = input;
    output.kind = HostTraceRecord::Kind::output;
    output.size = 2;
    output.bytes = {0xd0, 0x7f, 0};

    std::string text = hostTraceHeader();
    juce::MemoryBlock state("abc", 3);
    appendHostTraceStateLine(state, text);
    appendHostTraceLine(block, text);
    appendHostTraceLine(input, text);
    appendHostTraceLine(output, text);

    const auto parsed = parseHostTrace(text);
    CHECK(parsed.errorLine == 0);
    REQUIRE(parsed.blocks.size() == 1);
    const auto& b = parsed.blocks[0];
    CHECK(b.numSamples == 441);
    CHECK(b.sampleRate == 44100.0);
    CHECK(b.bypassed);
    CHECK(b.transport.isPlaying);
    CHECK(b.transport.hasMusicalPosition);
    CHECK(b.transport.playheadBeat == 0.1 + 0.2);
    CHECK(b.transport.tempoBeatsPerMinute == 133.33333333333334);
    CHECK(b.transport.isLooping);
    CHECK(b.transport.loopStartBeat == 4.0);
    CHECK(b.transport.loopEndBeat == 20.000000000000004);
    REQUIRE(b.stateBefore.has_value());
    CHECK(*b.stateBefore == state);
    REQUIRE(b.input.size() == 1);
    CHECK(b.input[0] == HostTraceMidi{17, {0x90, 0x3c, 0x64}});
    REQUIRE(b.output.size() == 1);
    CHECK(b.output[0] == HostTraceMidi{17, {0xd0, 0x7f}});
}

TEST_CASE("HostTrace: parser reports the first malformed line", "[HostTrace][M28]") {
    CHECK(parseHostTrace("# c\nI 0 90\n").errorLine == 2);  // MIDI before any block
    CHECK(parseHostTrace("B 64 48000 0 1 1 0 120 0 0\n").errorLine == 1);  // missing field
    CHECK(parseHostTrace("B 64 48000 0 1 1 0 120 0 0 0\nO 3 9x\n").errorLine == 2);
    CHECK(parseHostTrace("X\n").errorLine == 1);
}

TEST_CASE("HostTrace: recorded session replays to the identical plugin output", "[HostTrace][M28][D-59]") {
    // Live side: plugin state with one 1/8 lane, a looping host [0, 16) and 4096-sample blocks, so the
    // loop wraps inside a block (D-59) and the host retriggers its chord there.
    PluginRig live;
    PluginStateData state;
    for (auto& lane : state.parameters.lanes) {
        lane.mute = true;
    }
    state.parameters.lanes[0].mute = false;
    state.parameters.lanes[0].baseTimingIndex = 2;
    state.parameters.lanes[0].gatePercent = 84;
    applyState(state, live.parameters, live.patterns);
    live.patterns.setStepEnabled(0, 1, false);

    HostTraceRecorder recorder(1u << 14);
    const auto file = juce::File::createTempFile(".txt");
    std::vector<std::vector<HostTraceMidi>> liveOutputs;
    {
        HostTraceFileWriter writer(recorder, file, [&live](juce::MemoryBlock& out) { live.captureStateBlock(out); });
        constexpr int blockSize = 4096;
        constexpr std::int64_t loopEndSample = 16 * 24000;
        std::int64_t timelineSample = 0;
        for (int block = 0; block < 120; ++block) {
            const std::int64_t blockStart = timelineSample;
            const double ppq = static_cast<double>(blockStart % loopEndSample) / 24000.0;
            auto info = playingAt(ppq);
            info.isLooping = true;
            info.loopStartBeat = 0.0;
            info.loopEndBeat = 16.0;
            juce::MidiBuffer midi;
            if (block == 0) {
                midi.addEvent(juce::MidiMessage::noteOn(1, 38, static_cast<juce::uint8>(90)), 0);
            }
            const std::int64_t wrapInBlock = loopEndSample - (blockStart % loopEndSample);
            if (wrapInBlock > 0 && wrapInBlock < blockSize) {
                midi.addEvent(juce::MidiMessage::noteOff(1, 38), static_cast<int>(wrapInBlock));
                midi.addEvent(juce::MidiMessage::noteOn(1, 38, static_cast<juce::uint8>(90)), static_cast<int>(wrapInBlock));
            }
            recorder.beginBlock(blockSize, 48000.0, false, info);
            recorder.addMidi(HostTraceRecord::Kind::input, midi);
            live.process(midi, blockSize, info, 48000.0);
            recorder.addMidi(HostTraceRecord::Kind::output, midi);
            recorder.endBlock();
            liveOutputs.push_back(toTraceMidi(midi));
            timelineSample += blockSize;
            if (block == 0 || block == 60) {
                writer.flush();
            }
        }
    }  // writer destructor flushes the rest

    const auto parsed = parseHostTrace(readFile(file));
    (void)file.deleteFile();
    CHECK(parsed.errorLine == 0);
    REQUIRE(parsed.blocks.size() == liveOutputs.size());
    REQUIRE(parsed.blocks[0].stateBefore.has_value());
    std::size_t recordedEvents = 0;
    for (std::size_t i = 0; i < parsed.blocks.size(); ++i) {
        CHECK(parsed.blocks[i].output == liveOutputs[i]);
        recordedEvents += liveOutputs[i].size();
    }
    CHECK(recordedEvents > 10);
    CHECK(replay(parsed) == liveOutputs);
}

// Analysis tool for real host sessions (not run by default): build the plugin with
// -DPHU_ENABLE_HOST_TRACE=ON, record in the DAW, then run
//   SequentialAdapterTests "[.host-trace-replay]"   with PHU_HOST_TRACE_FILE=<trace file>
// It writes <trace file>.report.txt (transport jumps, every MIDI event with musical position and
// distance to the 1/16 grid, short notes) and checks that the current code reproduces the recording.
TEST_CASE("HostTrace: replay and report a recorded trace file", "[.host-trace-replay]") {
    const auto path = juce::SystemStats::getEnvironmentVariable("PHU_HOST_TRACE_FILE", {});
    if (path.isEmpty()) {
        SKIP("set PHU_HOST_TRACE_FILE to a trace file");
    }
    const juce::File file(path);
    const auto parsed = parseHostTrace(readFile(file));
    REQUIRE(parsed.errorLine == 0);
    const auto replayed = replay(parsed);

    std::ostringstream report;
    report.imbue(std::locale::classic());
    report.setf(std::ios::fixed);
    report.precision(4);
    std::int64_t hostSample = 0;
    double expectedPpq = 0;
    bool havePrevious = false;
    bool wasPlaying = false;
    std::size_t mismatches = 0;
    struct Open { std::int64_t sample; double beat; };
    std::vector<Open> openNotes(16 * 128, Open{-1, 0});

    const auto describe = [&](const char* tag, const HostTraceBlock& block, const HostTraceMidi& event) {
        const double spb = block.sampleRate * 60.0 / std::max(block.transport.tempoBeatsPerMinute, 1.0e-9);
        double beat = block.transport.playheadBeat + event.offset / spb;
        const auto& t = block.transport;
        if (t.isLooping && t.loopEndBeat > t.loopStartBeat && beat >= t.loopEndBeat - 1.0e-9) {
            beat -= t.loopEndBeat - t.loopStartBeat;
        }
        const double sixteenth = beat * 4.0;
        const double gridDeviationSamples = (sixteenth - std::round(sixteenth)) / 4.0 * spb;
        const std::int64_t absolute = hostSample + event.offset;
        report << "  " << tag << " @" << absolute << " (+" << event.offset << ") bar "
               << static_cast<std::int64_t>(std::floor(beat / 4.0)) + 1 << " beat " << std::fmod(beat, 4.0) + 1 << ' ';
        const int status = event.bytes.empty() ? 0 : event.bytes[0] & 0xf0;
        const int channel = event.bytes.empty() ? 0 : event.bytes[0] & 0x0f;
        if ((status == 0x90 || status == 0x80) && event.bytes.size() >= 3) {
            const bool on = status == 0x90 && event.bytes[2] > 0;
            report << (on ? "ON " : "OFF") << " ch" << channel + 1 << " p" << int{event.bytes[1]} << " v" << int{event.bytes[2]};
            if (on) {
                report << " grid16 " << gridDeviationSamples << " smp";
            }
            if (std::string(tag) == "out") {
                auto& open = openNotes[static_cast<std::size_t>(channel * 128 + event.bytes[1])];
                if (on) {
                    open = {absolute, beat};
                } else if (open.sample >= 0) {
                    const auto length = absolute - open.sample;
                    report << " len " << length << " smp";
                    if (length < spb / 16.0) {
                        report << "  <-- SHORT";
                    }
                    open.sample = -1;
                }
            }
        } else {
            for (const auto byte : event.bytes) {
                report << ' ' << int{byte};
            }
        }
        report << '\n';
    };

    for (std::size_t i = 0; i < parsed.blocks.size(); ++i) {
        const auto& block = parsed.blocks[i];
        const auto& t = block.transport;
        const double spb = block.sampleRate * 60.0 / std::max(t.tempoBeatsPerMinute, 1.0e-9);
        const bool jump = havePrevious && t.isPlaying && wasPlaying && std::abs(t.playheadBeat - expectedPpq) * spb > 1.0;
        if (!havePrevious || t.isPlaying != wasPlaying || jump || block.stateBefore.has_value()) {
            report << "B#" << i << " @" << hostSample << " n=" << block.numSamples << " ppq " << t.playheadBeat
                   << (t.isPlaying ? " playing" : " stopped") << (t.isLooping ? " loop " : "");
            if (t.isLooping) {
                report << t.loopStartBeat << ".." << t.loopEndBeat;
            }
            if (jump) {
                report << "  JUMP from expected " << expectedPpq;
            }
            if (block.stateBefore.has_value()) {
                report << "  [state]";
            }
            report << '\n';
        }
        for (const auto& event : block.input) {
            describe("in ", block, event);
        }
        for (const auto& event : replayed[i]) {
            describe("out", block, event);
        }
        if (replayed[i] != block.output) {
            ++mismatches;
            report << "  MISMATCH: recorded output differs (" << block.output.size() << " recorded events):\n";
            for (const auto& event : block.output) {
                describe("rec", block, event);
            }
        }
        expectedPpq = t.playheadBeat + block.numSamples / spb;
        wasPlaying = t.isPlaying;
        havePrevious = true;
        hostSample += block.numSamples;
    }
    report << "blocks " << parsed.blocks.size() << ", blocks whose replay differs from the recording: " << mismatches << '\n';

    const auto reportFile = file.getSiblingFile(file.getFileName() + ".report.txt");
    REQUIRE(reportFile.replaceWithText(juce::String(report.str())));
    INFO("report: " << reportFile.getFullPathName().toStdString());
    CHECK(mismatches == 0);
}
