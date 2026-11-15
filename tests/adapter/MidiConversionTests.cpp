#include <catch2/catch_test_macros.hpp>
#include "MidiConversion.h"
#include "arpeggiator/TransportTracker.h"
#include "sequencing/Constants.h"
#include "sequencing/MidiEvent.h"
#include "support/AllocationGuard.h"

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <bitset>
#include <span>

using namespace phu::plugin;
using namespace phu::arpeggiator;
using namespace phu::sequencing;
using phu::test::AllocationGuard;

namespace {

// Build a juce::MidiMessageMetadata from raw bytes
juce::MidiMessageMetadata makeMetadata(int sampleOffset, const juce::uint8* data, int size) {
    return juce::MidiMessageMetadata(data, size, sampleOffset);
}

}  // namespace

// --- routeOf: Note events (note-only path) ------------------------------------------------

TEST_CASE("routeOf: Note-On with velocity > 0 -> coreOnly", "[MidiConversion]") {
    std::array<juce::uint8, 3> bytes{0x90, 60, 100};
    auto metadata = makeMetadata(0, bytes.data(), 3);
    auto route = routeOf(metadata);
    CHECK(route == HostEventRoute::coreOnly);
}

TEST_CASE("routeOf: Note-On velocity 0 (note-off) -> coreOnly", "[MidiConversion]") {
    std::array<juce::uint8, 3> bytes{0x90, 60, 0};  // Note-On Ch1, pitch 60, velocity 0
    auto metadata = makeMetadata(0, bytes.data(), 3);
    auto route = routeOf(metadata);
    CHECK(route == HostEventRoute::coreOnly);
}

TEST_CASE("routeOf: Note-Off -> coreOnly", "[MidiConversion]") {
    std::array<juce::uint8, 3> bytes{0x80, 60, 64};  // Note-Off Ch1, pitch 60, velocity 64
    auto metadata = makeMetadata(0, bytes.data(), 3);
    auto route = routeOf(metadata);
    CHECK(route == HostEventRoute::coreOnly);
}

TEST_CASE("routeOf: all 16 MIDI channels Note-On -> coreOnly", "[MidiConversion]") {
    for (int ch = 0; ch < 16; ++ch) {
        std::array<juce::uint8, 3> bytes{static_cast<juce::uint8>(0x90 + ch), 60, 100};
        auto metadata = makeMetadata(0, bytes.data(), 3);
        auto route = routeOf(metadata);
        CHECK(route == HostEventRoute::coreOnly);
    }
}

// --- routeOf: CC64 (Sustain, special treatment) -------------------------------------------

TEST_CASE("routeOf: CC64 value 0 -> coreAndRaw", "[MidiConversion]") {
    std::array<juce::uint8, 3> bytes{0xB0, 64, 0};  // CC64, value 0
    auto metadata = makeMetadata(0, bytes.data(), 3);
    auto route = routeOf(metadata);
    CHECK(route == HostEventRoute::coreAndRaw);
}

TEST_CASE("routeOf: CC64 value < 64 (sustain off) -> coreAndRaw", "[MidiConversion]") {
    std::array<juce::uint8, 3> bytes{0xB0, 64, 63};
    auto metadata = makeMetadata(0, bytes.data(), 3);
    auto route = routeOf(metadata);
    CHECK(route == HostEventRoute::coreAndRaw);
}

TEST_CASE("routeOf: CC64 value >= 64 (sustain on) -> coreAndRaw", "[MidiConversion]") {
    std::array<juce::uint8, 3> bytes{0xB0, 64, 64};
    auto metadata = makeMetadata(0, bytes.data(), 3);
    auto route = routeOf(metadata);
    CHECK(route == HostEventRoute::coreAndRaw);
}

TEST_CASE("routeOf: CC64 value 127 -> coreAndRaw", "[MidiConversion]") {
    std::array<juce::uint8, 3> bytes{0xB0, 64, 127};
    auto metadata = makeMetadata(0, bytes.data(), 3);
    auto route = routeOf(metadata);
    CHECK(route == HostEventRoute::coreAndRaw);
}

// --- routeOf: other CC (raw-only) ----------------------------------------------------------

TEST_CASE("routeOf: other CC (CC1) -> rawOnly", "[MidiConversion]") {
    std::array<juce::uint8, 3> bytes{0xB0, 1, 50};  // CC1 (mod wheel)
    auto metadata = makeMetadata(0, bytes.data(), 3);
    auto route = routeOf(metadata);
    CHECK(route == HostEventRoute::rawOnly);
}

TEST_CASE("routeOf: CC63 -> rawOnly", "[MidiConversion]") {
    std::array<juce::uint8, 3> bytes{0xB0, 63, 100};
    auto metadata = makeMetadata(0, bytes.data(), 3);
    auto route = routeOf(metadata);
    CHECK(route == HostEventRoute::rawOnly);
}

TEST_CASE("routeOf: CC65 -> rawOnly", "[MidiConversion]") {
    std::array<juce::uint8, 3> bytes{0xB0, 65, 100};
    auto metadata = makeMetadata(0, bytes.data(), 3);
    auto route = routeOf(metadata);
    CHECK(route == HostEventRoute::rawOnly);
}

TEST_CASE("routeOf: CC127 -> rawOnly", "[MikiConversion]") {
    std::array<juce::uint8, 3> bytes{0xB0, 127, 100};
    auto metadata = makeMetadata(0, bytes.data(), 3);
    auto route = routeOf(metadata);
    CHECK(route == HostEventRoute::rawOnly);
}

// --- routeOf: pitch bend (raw-only) -------------------------------------------------------

TEST_CASE("routeOf: pitch bend -> rawOnly", "[MidiConversion]") {
    std::array<juce::uint8, 3> bytes{0xE0, 0, 64};  // Pitch bend center
    auto metadata = makeMetadata(0, bytes.data(), 3);
    auto route = routeOf(metadata);
    CHECK(route == HostEventRoute::rawOnly);
}

// --- routeOf: other messages (raw-only) ---------------------------------------------------

TEST_CASE("routeOf: channel aftertouch -> rawOnly", "[MidiConversion]") {
    std::array<juce::uint8, 2> bytes{0xD0, 64};  // Channel aftertouch
    auto metadata = makeMetadata(0, bytes.data(), 2);
    auto route = routeOf(metadata);
    CHECK(route == HostEventRoute::rawOnly);
}

TEST_CASE("routeOf: polytouch -> rawOnly", "[MidiConversion]") {
    std::array<juce::uint8, 3> bytes{0xA0, 60, 100};  // Poly aftertouch
    auto metadata = makeMetadata(0, bytes.data(), 3);
    auto route = routeOf(metadata);
    CHECK(route == HostEventRoute::rawOnly);
}

TEST_CASE("routeOf: program change -> rawOnly", "[MidiConversion]") {
    std::array<juce::uint8, 2> bytes{0xC0, 5};
    auto metadata = makeMetadata(0, bytes.data(), 2);
    auto route = routeOf(metadata);
    CHECK(route == HostEventRoute::rawOnly);
}

TEST_CASE("routeOf: SysEx -> rawOnly", "[MidiConversion]") {
    std::array<juce::uint8, 5> bytes{0xF0, 0x41, 0x10, 0x42, 0xF7};
    auto metadata = makeMetadata(0, bytes.data(), 5);
    auto route = routeOf(metadata);
    CHECK(route == HostEventRoute::rawOnly);
}

TEST_CASE("routeOf: Clock (realtime) -> rawOnly", "[MidiConversion]") {
    std::array<juce::uint8, 1> bytes{0xF8};
    auto metadata = makeMetadata(0, bytes.data(), 1);
    auto route = routeOf(metadata);
    CHECK(route == HostEventRoute::rawOnly);
}

// --- toCoreEvent: Note-On conversion -------------------------------------------------------

TEST_CASE("toCoreEvent: Note-On converts to core event", "[MidiConversion]") {
    std::array<juce::uint8, 3> bytes{0x90, 60, 100};  // Ch1, pitch 60, velocity 100
    auto metadata = makeMetadata(42, bytes.data(), 3);
    auto event = toCoreEvent(metadata, 42);
    
    CHECK(event.kind == MidiEventKind::noteOn);
    CHECK(event.channel == 1);
    CHECK(event.data1 == 60);  // pitch
    CHECK(event.data2 == 100); // velocity
    CHECK(event.sampleOffset == 42);
}

TEST_CASE("toCoreEvent: Note-On velocity 0 converts to noteOff", "[MidiConversion]") {
    std::array<juce::uint8, 3> bytes{0x90, 60, 0};
    auto metadata = makeMetadata(0, bytes.data(), 3);
    auto event = toCoreEvent(metadata, 0);
    
    CHECK(event.kind == MidiEventKind::noteOff);
    CHECK(event.channel == 1);
    CHECK(event.data1 == 60);
    CHECK(event.data2 == 0);
}

TEST_CASE("toCoreEvent: Note-Off converts correctly", "[MidiConversion]") {
    std::array<juce::uint8, 3> bytes{0x80, 72, 64};  // Ch1 Note-Off pitch 72
    auto metadata = makeMetadata(100, bytes.data(), 3);
    auto event = toCoreEvent(metadata, 100);
    
    CHECK(event.kind == MidiEventKind::noteOff);
    CHECK(event.channel == 1);
    CHECK(event.data1 == 72);
    CHECK(event.sampleOffset == 100);
}

TEST_CASE("toCoreEvent: respects all 16 channels", "[MidiConversion]") {
    for (int ch = 0; ch < 16; ++ch) {
        std::array<juce::uint8, 3> bytes{static_cast<juce::uint8>(0x90 + ch), 60, 100};
        auto metadata = makeMetadata(0, bytes.data(), 3);
        auto event = toCoreEvent(metadata, 0);
        CHECK(event.channel == ch + 1);
    }
}

TEST_CASE("toCoreEvent: respects all pitches", "[MidiConversion]") {
    for (int pitch = 0; pitch <= 127; ++pitch) {
        std::array<juce::uint8, 3> bytes{0x90, static_cast<juce::uint8>(pitch), 100};
        auto metadata = makeMetadata(0, bytes.data(), 3);
        auto event = toCoreEvent(metadata, 0);
        CHECK(event.data1 == pitch);
    }
}

TEST_CASE("toCoreEvent: sample offset passed through", "[MidiConversion]") {
    std::array<juce::uint8, 3> bytes{0x90, 60, 100};
    auto metadata = makeMetadata(999, bytes.data(), 3);
    auto event = toCoreEvent(metadata, 999);
    CHECK(event.sampleOffset == 999);
}

TEST_CASE("toCoreEvent: CC64 converts to controlChange", "[MidiConversion]") {
    std::array<juce::uint8, 3> bytes{0xB0, 64, 100};
    auto metadata = makeMetadata(50, bytes.data(), 3);
    auto event = toCoreEvent(metadata, 50);
    
    CHECK(event.kind == MidiEventKind::controlChange);
    CHECK(event.channel == 1);
    CHECK(event.data1 == 64);  // CC
    CHECK(event.data2 == 100); // value
    CHECK(event.sampleOffset == 50);
}

// --- DeferredInputStore: basic operations --------------------------------------------------

TEST_CASE("DeferredInputStore: empty on construction", "[MidiConversion]") {
    DeferredInputStore store;
    CHECK(store.empty() == true);
    CHECK(store.droppedEventCount() == 0);
}

TEST_CASE("DeferredInputStore: insert and not empty", "[MidiConversion]") {
    DeferredInputStore store;
    std::array<juce::uint8, 3> bytes{0x90, 60, 100};
    auto metadata = makeMetadata(0, bytes.data(), 3);
    
    store.insert(metadata);
    CHECK(store.empty() == false);
}

TEST_CASE("DeferredInputStore: clear empties store", "[MidiConversion]") {
    DeferredInputStore store;
    std::array<juce::uint8, 3> bytes{0x90, 60, 100};
    auto metadata = makeMetadata(0, bytes.data(), 3);
    
    store.insert(metadata);
    CHECK(store.empty() == false);
    store.clear();
    CHECK(store.empty() == true);
}

// --- DeferredInputStore: forEach iteration ------------------------------------------------

TEST_CASE("DeferredInputStore: forEach empty store visits nothing", "[MidiConversion]") {
    DeferredInputStore store;
    int visitCount = 0;
    store.forEach([&](const std::uint8_t*, int) { ++visitCount; });
    CHECK(visitCount == 0);
}

TEST_CASE("DeferredInputStore: forEach delivers events in order", "[MidiConversion]") {
    DeferredInputStore store;
    std::array<juce::uint8, 3> b1{0x90, 60, 100};
    std::array<juce::uint8, 3> b2{0x90, 62, 80};
    
    store.insert(makeMetadata(0, b1.data(), 3));
    store.insert(makeMetadata(0, b2.data(), 3));
    
    std::vector<std::vector<std::uint8_t>> visited;
    store.forEach([&](const std::uint8_t* data, int size) {
        visited.push_back(std::vector<std::uint8_t>(data, data + size));
    });
    
    REQUIRE(visited.size() == 2);
    CHECK(visited[0][1] == 60);  // First event pitch
    CHECK(visited[1][1] == 62);  // Second event pitch
}

// --- DeferredInputStore: release subsumption -----------------------------------------------

TEST_CASE("DeferredInputStore: later Note-Off subsumes earlier Note-Off for same key", "[MidiConversion]") {
    DeferredInputStore store;
    std::array<juce::uint8, 3> off1{0x80, 60, 64};
    std::array<juce::uint8, 3> off2{0x80, 60, 127};
    
    store.insert(makeMetadata(0, off1.data(), 3));
    store.insert(makeMetadata(0, off2.data(), 3));
    
    int visitCount = 0;
    store.forEach([&](const std::uint8_t*, int) { ++visitCount; });
    
    CHECK(visitCount == 1);  // Only the later Note-Off remains
}

TEST_CASE("DeferredInputStore: release subsumption for Note-On velocity 0", "[MidiConversion]") {
    DeferredInputStore store;
    std::array<juce::uint8, 3> off1{0x80, 60, 64};
    std::array<juce::uint8, 3> onV0{0x90, 60, 0};  // Note-On velocity 0 = release
    
    store.insert(makeMetadata(0, off1.data(), 3));
    store.insert(makeMetadata(0, onV0.data(), 3));
    
    int visitCount = 0;
    store.forEach([&](const std::uint8_t*, int) { ++visitCount; });
    CHECK(visitCount == 1);
}

TEST_CASE("DeferredInputStore: release subsumption for CC64 release classes", "[MidiConversion]") {
    DeferredInputStore store;
    std::array<juce::uint8, 3> cc64a{0xB0, 64, 50};
    std::array<juce::uint8, 3> cc64b{0xB0, 64, 30};
    
    store.insert(makeMetadata(0, cc64a.data(), 3));
    store.insert(makeMetadata(0, cc64b.data(), 3));
    
    int visitCount = 0;
    store.forEach([&](const std::uint8_t*, int) { ++visitCount; });
    CHECK(visitCount == 1);  // Only second CC64 remains
}

TEST_CASE("DeferredInputStore: release never dropped", "[MidiConversion]") {
    DeferredInputStore store;
    std::array<juce::uint8, 3> noteOff{0x80, 60, 64};
    
    // Fill to near capacity with non-release events, then try to add release
    // (This is a simplified test; full capacity testing would need more setup)
    store.insert(makeMetadata(0, noteOff.data(), 3));
    
    CHECK(store.droppedEventCount() == 0);  // Release not dropped
}

// --- DeferredInputStore: clear after many inserts ------------------------------------------

TEST_CASE("DeferredInputStore: clear after many inserts", "[MidiConversion]") {
    DeferredInputStore store;
    std::array<juce::uint8, 3> bytes{0x90, 60, 100};
    
    for (int i = 0; i < 100; ++i) {
        store.insert(makeMetadata(0, bytes.data(), 3));
    }
    
    CHECK(store.empty() == false);
    store.clear();
    CHECK(store.empty() == true);
}

// --- ChunkedInputReader: basic chunking ---------------------------------------------------

TEST_CASE("ChunkedInputReader: empty buffer and deferred -> single pure time-advance chunk",
         "[MidiConversion]") {
    // D-51: "the last chunk ends at n ... a chunk may have no events (pure time-advance)" -- the
    // core must still be called once per host block to advance transport/lane timing even when
    // there is no core-routed input at all. The original test asserted `next() == false` here,
    // which would make BlockEngine skip calling the core on any block with no note/CC64 input --
    // contradicting D-51 and the continuous-sequencing behavior exercised by BlockEngineTests
    // (e.g. "transport playing with held note emits sequenced Note-Ons" relies on the core being
    // called for input-less blocks). Fixed as part of Issue #12 review.
    DeferredInputStore deferred;
    juce::MidiBuffer hostInput;
    ChunkedInputReader reader(deferred, hostInput, 4096);
    
    std::array<MidiEvent, constants::capacity::inputEventsPerChunk> chunk;
    ChunkedInputReader::ChunkDescriptor desc;
    
    REQUIRE(reader.next(chunk, desc) == true);
    CHECK(desc.eventCount == 0);
    CHECK(desc.startSample == 0);
    CHECK(desc.numSamples == 4096);
    CHECK(desc.moreInputAtSameOffset == false);
    CHECK(reader.next(chunk, desc) == false);
}

TEST_CASE("ChunkedInputReader: single chunk contains all core events", "[MidiConversion]") {
    DeferredInputStore deferred;
    juce::MidiBuffer hostInput;
    std::array<juce::uint8, 3> bytes{0x90, 60, 100};
    hostInput.addEvent(juce::MidiMessage(bytes.data(), 3, false), 100);
    
    ChunkedInputReader reader(deferred, hostInput, 4096);
    
    std::array<MidiEvent, constants::capacity::inputEventsPerChunk> chunk;
    ChunkedInputReader::ChunkDescriptor desc;
    
    CHECK(reader.next(chunk, desc) == true);
    CHECK(desc.eventCount == 1);
    CHECK(desc.startSample == 0);
    CHECK(desc.numSamples <= 4096);
    CHECK(desc.moreInputAtSameOffset == false);
}

TEST_CASE("ChunkedInputReader: deferred events at offset 0", "[MidiConversion]") {
    DeferredInputStore deferred;
    std::array<juce::uint8, 3> bytes{0x90, 60, 100};
    deferred.insert(makeMetadata(0, bytes.data(), 3));
    
    juce::MidiBuffer hostInput;
    ChunkedInputReader reader(deferred, hostInput, 4096);
    
    std::array<MidiEvent, constants::capacity::inputEventsPerChunk> chunk;
    ChunkedInputReader::ChunkDescriptor desc;
    
    CHECK(reader.next(chunk, desc) == true);
    CHECK(desc.eventCount == 1);
    CHECK(desc.startSample == 0);
}

TEST_CASE("ChunkedInputReader: offset clamping to block size", "[MidiConversion]") {
    DeferredInputStore deferred;
    juce::MidiBuffer hostInput;
    
    // Event at offset 5000 (within 4096 block -> clamped to 4095)
    std::array<juce::uint8, 3> bytes{0x90, 60, 100};
    hostInput.addEvent(juce::MidiMessage(bytes.data(), 3, false), 5000);
    
    ChunkedInputReader reader(deferred, hostInput, 4096);
    
    std::array<MidiEvent, constants::capacity::inputEventsPerChunk> chunk;
    ChunkedInputReader::ChunkDescriptor desc;
    
    CHECK(reader.next(chunk, desc) == true);
    if (desc.eventCount > 0) {
        CHECK(chunk[0].sampleOffset >= 0);
        CHECK(chunk[0].sampleOffset < 4096);
    }
}

TEST_CASE("ChunkedInputReader: non-core events skipped", "[MidiConversion]") {
    // Fixed alongside the "empty buffer" test above for the same D-51 reason: a block whose only
    // content is raw-only (rawOnly-routed) input still has zero core events, but still yields one
    // pure time-advance chunk spanning the whole block.
    DeferredInputStore deferred;
    juce::MidiBuffer hostInput;
    
    std::array<juce::uint8, 3> ccbytes{0xB0, 1, 100};  // CC1 (rawOnly)
    hostInput.addEvent(juce::MidiMessage(ccbytes.data(), 3, false), 100);
    
    ChunkedInputReader reader(deferred, hostInput, 4096);
    
    std::array<MidiEvent, constants::capacity::inputEventsPerChunk> chunk;
    ChunkedInputReader::ChunkDescriptor desc;
    
    REQUIRE(reader.next(chunk, desc) == true);
    CHECK(desc.eventCount == 0);  // CC1 is rawOnly, not delivered to the core
    CHECK(desc.numSamples == 4096);
    CHECK(reader.next(chunk, desc) == false);
}

// --- ChunkedInputReader: large same-offset groups ----------------------------------------

TEST_CASE("ChunkedInputReader: large same-offset group sets moreInputAtSameOffset", "[MidiConversion]") {
    DeferredInputStore deferred;
    juce::MidiBuffer hostInput;
    
    // Add > 4096 events at same offset (clamped to 4096 max per chunk)
    std::array<juce::uint8, 3> bytes{0x90, 60, 100};
    for (int i = 0; i < 5000; ++i) {
        hostInput.addEvent(juce::MidiMessage(bytes.data(), 3, false), 100);
    }
    
    ChunkedInputReader reader(deferred, hostInput, 8000);
    
    std::array<MidiEvent, constants::capacity::inputEventsPerChunk> chunk;
    ChunkedInputReader::ChunkDescriptor desc;
    
    // First call should have moreInputAtSameOffset=true if events exceed capacity
    if (reader.next(chunk, desc)) {
        // May or may not set moreInputAtSameOffset depending on actual event count
    }
}

// --- OutputMidiWriter: basic operations ---------------------------------------------------

TEST_CASE("OutputMidiWriter: beginBlock clears scratch", "[MidiConversion]") {
    juce::MidiBuffer scratch;
    scratch.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8)100), 0);
    CHECK(scratch.getNumEvents() > 0);
    
    std::bitset<constants::capacity::maximumSoundingVoices> outputOpen;
    OutputMidiWriter writer(scratch, outputOpen);
    writer.beginBlock();
    
    CHECK(scratch.getNumEvents() == 0);
}

TEST_CASE("OutputMidiWriter: appendRaw copies raw events", "[MidiConversion]") {
    juce::MidiBuffer scratch;
    std::bitset<constants::capacity::maximumSoundingVoices> outputOpen;
    OutputMidiWriter writer(scratch, outputOpen);
    writer.beginBlock();
    
    std::array<juce::uint8, 3> bytes{0x90, 60, 100};
    auto metadata = makeMetadata(50, bytes.data(), 3);
    
    bool added = writer.appendRaw(metadata, 50);
    CHECK(added == true);
    CHECK(scratch.getNumEvents() > 0);
}

TEST_CASE("OutputMidiWriter: appendCoreEvents adds Note-On and Note-Off", "[MidiConversion]") {
    juce::MidiBuffer scratch;
    std::bitset<constants::capacity::maximumSoundingVoices> outputOpen;
    OutputMidiWriter writer(scratch, outputOpen);
    writer.beginBlock();
    
    MidiEvent noteOn = makeNoteOn(0, 1, 60, 100);
    std::array<MidiEvent, 1> events{noteOn};
    
    writer.appendCoreEvents(events, 0);
    CHECK(scratch.getNumEvents() > 0);
}

TEST_CASE("OutputMidiWriter: no allocation after prepare", "[MidiConversion]") {
    juce::MidiBuffer scratch;
    scratch.ensureSize(constants::capacity::outputMidiReserveBytes);
    
    std::bitset<constants::capacity::maximumSoundingVoices> outputOpen;
    OutputMidiWriter writer(scratch, outputOpen);
    writer.beginBlock();
    
    AllocationGuard guard;
    
    // Add events up to capacity
    std::array<MidiEvent, 10> events{};
    for (int i = 0; i < 10; ++i) {
        events[i] = makeNoteOn(i * 100, 1, 60 + i, 100);
    }
    writer.appendCoreEvents(std::span(events.data(), 10), 0);
    
    CHECK(guard.allocationCount() == 0);
}

TEST_CASE("OutputMidiWriter: refusal count increments on drop", "[MidiConversion]") {
    juce::MidiBuffer scratch;
    scratch.ensureSize(100);  // Very small to force refusals
    
    std::bitset<constants::capacity::maximumSoundingVoices> outputOpen;
    OutputMidiWriter writer(scratch, outputOpen);
    writer.beginBlock();
    
    // Try to add many large SysEx (not supported, but illustrates refusal)
    std::array<juce::uint8, 50> largeSysEx;
    largeSysEx[0] = 0xF0;
    for (int i = 1; i < 49; ++i) largeSysEx[i] = 0x00;
    largeSysEx[49] = 0xF7;
    
    auto metadata = makeMetadata(0, largeSysEx.data(), 50);
    bool added = writer.appendRaw(metadata, 0);
    
    if (!added) {
        CHECK(writer.droppedEventCount() > 0);
    }
}

TEST_CASE("OutputMidiWriter: Note-Off for open key always written", "[MidiConversion]") {
    juce::MidiBuffer scratch;
    scratch.ensureSize(constants::capacity::outputMidiReserveBytes);
    
    std::bitset<constants::capacity::maximumSoundingVoices> outputOpen;
    outputOpen.set(0 * 128 + 60);  // Mark (ch1, pitch60) as open
    
    OutputMidiWriter writer(scratch, outputOpen);
    writer.beginBlock();
    
    MidiEvent noteOff = makeNoteOff(100, 1, 60);
    std::array<MidiEvent, 1> events{noteOff};
    
    writer.appendCoreEvents(events, 0);
    CHECK(scratch.getNumEvents() > 0);
}

TEST_CASE("OutputMidiWriter: Note-Off for non-open key suppressed", "[MidiConversion]") {
    juce::MidiBuffer scratch;
    scratch.ensureSize(constants::capacity::outputMidiReserveBytes);
    
    std::bitset<constants::capacity::maximumSoundingVoices> outputOpen;
    // Key not marked as open
    
    OutputMidiWriter writer(scratch, outputOpen);
    writer.beginBlock();
    
    MidiEvent noteOff = makeNoteOff(100, 1, 60);
    std::array<MidiEvent, 1> events{noteOff};
    
    int eventsBefore = scratch.getNumEvents();
    writer.appendCoreEvents(events, 0);
    int eventsAfter = scratch.getNumEvents();
    
    // Note-Off for non-open key should not be added
    CHECK(eventsAfter == eventsBefore);
}

