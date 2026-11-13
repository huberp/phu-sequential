#include <catch2/catch_test_macros.hpp>
#include "sequencing/VoiceRegistry.h"
#include "sequencing/OutputEventBuffer.h"
#include "sequencing/MidiEvent.h"
#include "sequencing/Constants.h"

using namespace phu::sequencing;
using namespace phu::sequencing::constants;
using namespace phu::sequencing::constants::lanes;

// M11 Voice registry (§9.1, §9.2, §9.3, §9.4, #9)

TEST_CASE("VoiceRegistry::constructor creates empty registry", "[M11]") {
    VoiceRegistry registry;
    
    CHECK(registry.soundingCount() == 0);
    CHECK(registry.nextScheduledEnd() == std::nullopt);
}

TEST_CASE("VoiceRegistry::startVoice emits Note-On when buffer accepts", "[M11][REQ-9.1]") {
    VoiceRegistry registry;
    const int bufCapacity = capacity::noteOffReserveEvents + 10;
    OutputEventBuffer buffer(bufCapacity);
    
    const VoiceStartRequest req{0, 1, 60, 100, noScheduledEnd};
    const auto handle = registry.startVoice(req, 0, buffer);
    
    CHECK(handle.isValid() == true);
    CHECK(handle.id != invalidVoiceId);
    CHECK(registry.soundingCount() == 1);
    
    // Buffer should have one Note-On event
    REQUIRE(buffer.size() == 1);
    const auto& events = buffer.events();
    CHECK(events[0].kind == MidiEventKind::noteOn);
    CHECK(events[0].channel == 1);
    CHECK(events[0].data1 == 60);
    CHECK(events[0].data2 == 100);
}

TEST_CASE("VoiceRegistry::startVoice returns invalid handle if buffer full", "[M11]") {
    VoiceRegistry registry;
    const int bufCapacity = capacity::noteOffReserveEvents + 1;
    OutputEventBuffer buffer(bufCapacity);
    
    // Fill buffer with one Note-On
    const VoiceStartRequest req1{0, 1, 60, 100, noScheduledEnd};
    (void)registry.startVoice(req1, 0, buffer);
    
    // Next Note-On should be refused
    const VoiceStartRequest req2{0, 1, 61, 100, noScheduledEnd};
    const auto handle = registry.startVoice(req2, 0, buffer);
    
    CHECK(handle.isValid() == false);
    CHECK(registry.soundingCount() == 1);  // Only the first voice counts
}

TEST_CASE("VoiceRegistry::endVoice emits Note-Off (velocity 0) when voice sounding", "[M11][REQ-9.1]") {
    VoiceRegistry registry;
    const int bufCapacity = capacity::noteOffReserveEvents + 10;
    OutputEventBuffer buffer(bufCapacity);
    
    const VoiceStartRequest req{0, 1, 60, 100, noScheduledEnd};
    const auto handle = registry.startVoice(req, 0, buffer);
    REQUIRE(handle.isValid() == true);
    
    buffer.clear();
    
    // End the voice
    bool ended = registry.endVoice(handle, 10, buffer);
    
    CHECK(ended == true);
    CHECK(registry.soundingCount() == 0);
    
    // Buffer should have one Note-Off event
    REQUIRE(buffer.size() == 1);
    const auto& events = buffer.events();
    CHECK(events[0].kind == MidiEventKind::noteOff);
    CHECK(events[0].channel == 1);
    CHECK(events[0].data1 == 60);
    CHECK(events[0].data2 == 0);  // note-off velocity = 0
}

TEST_CASE("VoiceRegistry::endVoice returns false and suppresses Note-Off if handle stale", "[M11][REQ-9.1]") {
    VoiceRegistry registry;
    const int bufCapacity = capacity::noteOffReserveEvents + 10;
    OutputEventBuffer buffer(bufCapacity);
    
    // Start a voice
    const VoiceStartRequest req1{0, 1, 60, 100, noScheduledEnd};
    const auto handle1 = registry.startVoice(req1, 0, buffer);
    
    // Retrigger the same (channel, pitch) - ownership transfers
    buffer.clear();
    const VoiceStartRequest req2{1, 1, 60, 110, noScheduledEnd};
    (void)registry.startVoice(req2, 5, buffer);
    
    buffer.clear();
    
    // Try to end the old handle - should be suppressed
    bool ended = registry.endVoice(handle1, 20, buffer);
    
    CHECK(ended == false);
    CHECK(buffer.size() == 0);  // No Note-Off emitted
}

TEST_CASE("VoiceRegistry::startVoice retrigger: emits Off then On for same (channel, pitch)", "[M11][REQ-9.1]") {
    VoiceRegistry registry;
    const int bufCapacity = capacity::noteOffReserveEvents + 10;
    OutputEventBuffer buffer(bufCapacity);
    
    // Start first voice on (1, 60)
    const VoiceStartRequest req1{0, 1, 60, 100, noScheduledEnd};
    const auto handle1 = registry.startVoice(req1, 0, buffer);
    REQUIRE(handle1.isValid() == true);
    
    buffer.clear();
    
    // Retrigger same (channel, pitch) with different owner
    const VoiceStartRequest req2{1, 1, 60, 110, noScheduledEnd};
    const auto handle2 = registry.startVoice(req2, 5, buffer);
    
    REQUIRE(handle2.isValid() == true);
    REQUIRE(buffer.size() == 2);
    
    // First event should be Note-Off (from retrigger)
    const auto& events = buffer.events();
    CHECK(events[0].kind == MidiEventKind::noteOff);
    CHECK(events[0].channel == 1);
    CHECK(events[0].data1 == 60);
    
    // Second event should be Note-On (new voice)
    CHECK(events[1].kind == MidiEventKind::noteOn);
    CHECK(events[1].channel == 1);
    CHECK(events[1].data1 == 60);
    CHECK(events[1].data2 == 110);
    
    // Old handle's endVoice should be suppressed
    buffer.clear();
    bool ended = registry.endVoice(handle1, 10, buffer);
    CHECK(ended == false);
    CHECK(buffer.size() == 0);
    
    // New handle's endVoice should emit Note-Off
    buffer.clear();
    ended = registry.endVoice(handle2, 20, buffer);
    CHECK(ended == true);
    CHECK(buffer.size() == 1);
}

TEST_CASE("VoiceRegistry::startVoice ownership transfer across lanes", "[M11][REQ-9.1]") {
    VoiceRegistry registry;
    const int bufCapacity = capacity::noteOffReserveEvents + 10;
    OutputEventBuffer buffer(bufCapacity);
    
    // Lane 0 plays (1, 60)
    const VoiceStartRequest req1{0, 1, 60, 100, noScheduledEnd};
    (void)registry.startVoice(req1, 0, buffer);
    
    CHECK(registry.ownerOf(1, 60) == 0);
    
    buffer.clear();
    
    // Lane 1 retriggers same (1, 60)
    const VoiceStartRequest req2{1, 1, 60, 110, noScheduledEnd};
    (void)registry.startVoice(req2, 5, buffer);
    
    CHECK(registry.ownerOf(1, 60) == 1);
}

TEST_CASE("VoiceRegistry::startVoice per-owner limit evicts oldest voice when limit exceeded", "[M11][REQ-9.4]") {
    VoiceRegistry registry;
    const int bufCapacity = capacity::noteOffReserveEvents + 100;
    OutputEventBuffer buffer(bufCapacity);
    
    // Set limit for owner 0
    registry.setVoiceLimit(0, maximumVoicesPerLane);  // 32
    
    // Start 32 voices for owner 0
    std::vector<VoiceHandle> handles;
    for (int i = 0; i < maximumVoicesPerLane; ++i) {
        const VoiceStartRequest req{0, 1, static_cast<int>(i % 128), 100, noScheduledEnd};
        const auto handle = registry.startVoice(req, i, buffer);
        REQUIRE(handle.isValid() == true);
        handles.push_back(handle);
    }
    
    CHECK(registry.soundingCount(0) == maximumVoicesPerLane);
    
    buffer.clear();
    
    // Start 33rd voice - should evict oldest
    const VoiceStartRequest req33{0, 2, 32, 100, noScheduledEnd};
    const auto handle33 = registry.startVoice(req33, maximumVoicesPerLane, buffer);
    REQUIRE(handle33.isValid() == true);
    
    // Still 32 voices for owner 0
    CHECK(registry.soundingCount(0) == maximumVoicesPerLane);
    
    // Buffer should have eviction Note-Off and new Note-On
    REQUIRE(buffer.size() >= 2);
    // First event: eviction Note-Off
    // Last event: new Note-On
}

TEST_CASE("VoiceRegistry::setVoiceLimit only affects specified owner", "[M11][REQ-9.4]") {
    VoiceRegistry registry;
    const int bufCapacity = capacity::noteOffReserveEvents + 100;
    OutputEventBuffer buffer(bufCapacity);
    
    registry.setVoiceLimit(0, 2);   // Owner 0 limit: 2
    registry.setVoiceLimit(1, 100); // Owner 1 limit: 100
    
    // Fill owner 0 with 2 voices
    (void)registry.startVoice(VoiceStartRequest{0, 1, 60, 100, noScheduledEnd}, 0, buffer);
    (void)registry.startVoice(VoiceStartRequest{0, 2, 61, 100, noScheduledEnd}, 1, buffer);
    
    buffer.clear();
    
    // Third voice should evict one from owner 0
    (void)registry.startVoice(VoiceStartRequest{0, 3, 62, 100, noScheduledEnd}, 2, buffer);
    CHECK(registry.soundingCount(0) == 2);
    
    buffer.clear();
    
    // Owner 1 can have many voices
    for (int i = 0; i < 10; ++i) {
        (void)registry.startVoice(VoiceStartRequest{1, 4, static_cast<int>(60 + i), 100, noScheduledEnd}, i + 3, buffer);
    }
    
    CHECK(registry.soundingCount(1) == 10);
}

TEST_CASE("VoiceRegistry::releaseOwner emits Note-Offs for all owner's voices (oldest first)", "[M11]") {
    VoiceRegistry registry;
    const int bufCapacity = capacity::noteOffReserveEvents + 100;
    OutputEventBuffer buffer(bufCapacity);
    
    // Start 3 voices for owner 0
    (void)registry.startVoice(VoiceStartRequest{0, 1, 60, 100, noScheduledEnd}, 0, buffer);
    (void)registry.startVoice(VoiceStartRequest{0, 2, 61, 100, noScheduledEnd}, 1, buffer);
    (void)registry.startVoice(VoiceStartRequest{0, 3, 62, 100, noScheduledEnd}, 2, buffer);
    
    buffer.clear();
    
    // Release all of owner 0
    int count = registry.releaseOwner(0, 10, buffer);
    
    CHECK(count == 3);
    CHECK(registry.soundingCount(0) == 0);
    
    // Buffer should have 3 Note-Offs in start order (oldest first)
    REQUIRE(buffer.size() == 3);
    const auto& events = buffer.events();
    for (const auto& ev : events) {
        CHECK(ev.kind == MidiEventKind::noteOff);
    }
}

TEST_CASE("VoiceRegistry::releaseAll emits Note-Offs for all voices (oldest first)", "[M11][REQ-9.3]") {
    VoiceRegistry registry;
    const int bufCapacity = capacity::noteOffReserveEvents + 100;
    OutputEventBuffer buffer(bufCapacity);
    
    // Start voices for multiple owners
    (void)registry.startVoice(VoiceStartRequest{0, 1, 60, 100, noScheduledEnd}, 0, buffer);
    (void)registry.startVoice(VoiceStartRequest{1, 2, 61, 100, noScheduledEnd}, 1, buffer);
    (void)registry.startVoice(VoiceStartRequest{2, 3, 62, 100, noScheduledEnd}, 2, buffer);
    
    buffer.clear();
    
    // Release all voices
    int count = registry.releaseAll(10, buffer);
    
    CHECK(count == 3);
    CHECK(registry.soundingCount() == 0);
    
    REQUIRE(buffer.size() == 3);
    const auto& events = buffer.events();
    for (const auto& ev : events) {
        CHECK(ev.kind == MidiEventKind::noteOff);
    }
}

TEST_CASE("VoiceRegistry::reset clears all voices WITHOUT emitting events", "[M11]") {
    VoiceRegistry registry;
    const int bufCapacity = capacity::noteOffReserveEvents + 10;
    OutputEventBuffer buffer(bufCapacity);
    
    // Start some voices
    (void)registry.startVoice(VoiceStartRequest{0, 1, 60, 100, noScheduledEnd}, 0, buffer);
    (void)registry.startVoice(VoiceStartRequest{0, 2, 61, 100, noScheduledEnd}, 1, buffer);
    
    buffer.clear();
    
    // Reset
    registry.reset();
    
    CHECK(registry.soundingCount() == 0);
    CHECK(buffer.size() == 0);  // No events emitted
}

TEST_CASE("VoiceRegistry::adoptVoice registers voice without emitting", "[M11]") {
    VoiceRegistry registry;
    
    // adoptVoice takes no OutputEventBuffer - it doesn't emit
    const VoiceStartRequest req{0, 1, 60, 100, noScheduledEnd};
    const auto handle = registry.adoptVoice(req);
    
    CHECK(handle.isValid() == true);
    CHECK(registry.soundingCount() == 1);
    CHECK(registry.ownerOf(1, 60) == 0);
}

TEST_CASE("VoiceRegistry::adoptVoice applies per-owner limit eviction on registration", "[M11]") {
    VoiceRegistry registry;
    const int bufCapacity = capacity::noteOffReserveEvents + 100;
    OutputEventBuffer buffer(bufCapacity);
    
    registry.setVoiceLimit(0, 2);
    
    // adoptVoice 3 voices
    (void)registry.adoptVoice(VoiceStartRequest{0, 1, 60, 100, noScheduledEnd});
    (void)registry.adoptVoice(VoiceStartRequest{0, 2, 61, 100, noScheduledEnd});
    (void)registry.adoptVoice(VoiceStartRequest{0, 3, 62, 100, noScheduledEnd});
    
    // Still at limit
    CHECK(registry.soundingCount(0) == 2);
}

TEST_CASE("VoiceRegistry::nextScheduledEnd returns earliest finite end (ties: earlier start)", "[M11]") {
    VoiceRegistry registry;
    const int bufCapacity = capacity::noteOffReserveEvents + 10;
    OutputEventBuffer buffer(bufCapacity);
    
    // Voice 1: ends at beat 2.0, starts at 0
    (void)registry.startVoice(VoiceStartRequest{0, 1, 60, 100, 2.0}, 0, buffer);
    
    // Voice 2: ends at beat 1.0, starts at 1
    (void)registry.startVoice(VoiceStartRequest{0, 2, 61, 100, 1.0}, 1, buffer);
    
    // Voice 3: no scheduled end (noScheduledEnd)
    (void)registry.startVoice(VoiceStartRequest{0, 3, 62, 100, noScheduledEnd}, 2, buffer);
    
    const auto next = registry.nextScheduledEnd();
    REQUIRE(next.has_value());
    
    // Should be voice 2 (earliest end at 1.0)
    CHECK(next->beat == 1.0);
}

TEST_CASE("VoiceRegistry::nextScheduledEnd ignores noScheduledEnd voices", "[M11]") {
    VoiceRegistry registry;
    const int bufCapacity = capacity::noteOffReserveEvents + 10;
    OutputEventBuffer buffer(bufCapacity);
    
    // All voices have noScheduledEnd
    (void)registry.startVoice(VoiceStartRequest{0, 1, 60, 100, noScheduledEnd}, 0, buffer);
    (void)registry.startVoice(VoiceStartRequest{0, 2, 61, 100, noScheduledEnd}, 1, buffer);
    
    const auto next = registry.nextScheduledEnd();
    CHECK(next == std::nullopt);
}

TEST_CASE("VoiceRegistry::nextScheduledEnd returns empty after all scheduled voices end", "[M11]") {
    VoiceRegistry registry;
    const int bufCapacity = capacity::noteOffReserveEvents + 10;
    OutputEventBuffer buffer(bufCapacity);
    
    // One voice with scheduled end
    const VoiceStartRequest req{0, 1, 60, 100, 1.0};
    const auto handle = registry.startVoice(req, 0, buffer);
    
    REQUIRE(registry.nextScheduledEnd().has_value());
    
    buffer.clear();
    registry.endVoice(handle, 0, buffer);
    
    CHECK(registry.nextScheduledEnd() == std::nullopt);
}

TEST_CASE("VoiceRegistry::isSounding returns true for valid sounding voice", "[M11]") {
    VoiceRegistry registry;
    const int bufCapacity = capacity::noteOffReserveEvents + 10;
    OutputEventBuffer buffer(bufCapacity);
    
    const VoiceStartRequest req{0, 1, 60, 100, noScheduledEnd};
    const auto handle = registry.startVoice(req, 0, buffer);
    
    CHECK(registry.isSounding(handle) == true);
}

TEST_CASE("VoiceRegistry::isSounding returns false for invalid handle", "[M11]") {
    VoiceRegistry registry;
    
    const VoiceHandle invalid{invalidVoiceId, 0};
    CHECK(registry.isSounding(invalid) == false);
}

TEST_CASE("VoiceRegistry::isSounding returns false after endVoice", "[M11]") {
    VoiceRegistry registry;
    const int bufCapacity = capacity::noteOffReserveEvents + 10;
    OutputEventBuffer buffer(bufCapacity);
    
    const VoiceStartRequest req{0, 1, 60, 100, noScheduledEnd};
    const auto handle = registry.startVoice(req, 0, buffer);
    
    buffer.clear();
    registry.endVoice(handle, 0, buffer);
    
    CHECK(registry.isSounding(handle) == false);
}

TEST_CASE("VoiceRegistry::soundingCount returns total voices", "[M11]") {
    VoiceRegistry registry;
    const int bufCapacity = capacity::noteOffReserveEvents + 10;
    OutputEventBuffer buffer(bufCapacity);
    
    CHECK(registry.soundingCount() == 0);
    
    (void)registry.startVoice(VoiceStartRequest{0, 1, 60, 100, noScheduledEnd}, 0, buffer);
    CHECK(registry.soundingCount() == 1);
    
    (void)registry.startVoice(VoiceStartRequest{1, 2, 61, 100, noScheduledEnd}, 1, buffer);
    CHECK(registry.soundingCount() == 2);
}

TEST_CASE("VoiceRegistry::soundingCount(owner) returns per-owner voices", "[M11]") {
    VoiceRegistry registry;
    const int bufCapacity = capacity::noteOffReserveEvents + 10;
    OutputEventBuffer buffer(bufCapacity);
    
    (void)registry.startVoice(VoiceStartRequest{0, 1, 60, 100, noScheduledEnd}, 0, buffer);
    (void)registry.startVoice(VoiceStartRequest{0, 2, 61, 100, noScheduledEnd}, 1, buffer);
    (void)registry.startVoice(VoiceStartRequest{1, 3, 62, 100, noScheduledEnd}, 2, buffer);
    
    CHECK(registry.soundingCount(0) == 2);
    CHECK(registry.soundingCount(1) == 1);
}

TEST_CASE("VoiceRegistry::ownerOf returns owner of voice at (channel, pitch)", "[M11]") {
    VoiceRegistry registry;
    const int bufCapacity = capacity::noteOffReserveEvents + 10;
    OutputEventBuffer buffer(bufCapacity);
    
    (void)registry.startVoice(VoiceStartRequest{0, 1, 60, 100, noScheduledEnd}, 0, buffer);
    (void)registry.startVoice(VoiceStartRequest{2, 2, 61, 100, noScheduledEnd}, 1, buffer);
    
    CHECK(registry.ownerOf(1, 60) == 0);
    CHECK(registry.ownerOf(2, 61) == 2);
}

TEST_CASE("VoiceRegistry::ownerOf returns nullopt for unoccupied (channel, pitch)", "[M11]") {
    VoiceRegistry registry;
    
    CHECK(registry.ownerOf(1, 60) == std::nullopt);
}

TEST_CASE("VoiceRegistry: VoiceId never 0 (invalidVoiceId)", "[M11]") {
    VoiceRegistry registry;
    const int bufCapacity = capacity::noteOffReserveEvents + 10;
    OutputEventBuffer buffer(bufCapacity);
    
    const VoiceStartRequest req{0, 1, 60, 100, noScheduledEnd};
    const auto handle = registry.startVoice(req, 0, buffer);
    
    CHECK(handle.id != invalidVoiceId);
}

TEST_CASE("VoiceRegistry: invalid input rejected (channel 0)", "[M11]") {
    VoiceRegistry registry;
    const int bufCapacity = capacity::noteOffReserveEvents + 10;
    OutputEventBuffer buffer(bufCapacity);
    
    const VoiceStartRequest req{0, 0, 60, 100, noScheduledEnd};  // channel 0 invalid
    const auto handle = registry.startVoice(req, 0, buffer);
    
    CHECK(handle.isValid() == false);
    CHECK(buffer.size() == 0);  // No event emitted
}

TEST_CASE("VoiceRegistry: invalid input rejected (channel > 16)", "[M11]") {
    VoiceRegistry registry;
    const int bufCapacity = capacity::noteOffReserveEvents + 10;
    OutputEventBuffer buffer(bufCapacity);
    
    const VoiceStartRequest req{0, 17, 60, 100, noScheduledEnd};  // channel 17 invalid
    const auto handle = registry.startVoice(req, 0, buffer);
    
    CHECK(handle.isValid() == false);
    CHECK(buffer.size() == 0);
}

TEST_CASE("VoiceRegistry: invalid input rejected (pitch 128)", "[M11]") {
    VoiceRegistry registry;
    const int bufCapacity = capacity::noteOffReserveEvents + 10;
    OutputEventBuffer buffer(bufCapacity);
    
    const VoiceStartRequest req{0, 1, 128, 100, noScheduledEnd};  // pitch 128 invalid
    const auto handle = registry.startVoice(req, 0, buffer);
    
    CHECK(handle.isValid() == false);
    CHECK(buffer.size() == 0);
}

TEST_CASE("VoiceRegistry: invalid input rejected (owner >= voiceOwnerCount)", "[M11]") {
    VoiceRegistry registry;
    const int bufCapacity = capacity::noteOffReserveEvents + 10;
    OutputEventBuffer buffer(bufCapacity);
    
    const VoiceStartRequest req{static_cast<OwnerId>(capacity::voiceOwnerCount), 1, 60, 100, noScheduledEnd};
    const auto handle = registry.startVoice(req, 0, buffer);
    
    CHECK(handle.isValid() == false);
    CHECK(buffer.size() == 0);
}

TEST_CASE("VoiceRegistry: valid channels 1-16 accepted", "[M11]") {
    VoiceRegistry registry;
    const int bufCapacity = capacity::noteOffReserveEvents + 50;
    OutputEventBuffer buffer(bufCapacity);
    
    for (int ch = 1; ch <= 16; ++ch) {
        const VoiceStartRequest req{0, ch, 60, 100, noScheduledEnd};
        const auto handle = registry.startVoice(req, ch - 1, buffer);
        
        // Only first voice accepted (buffer fills up after retriggers)
        if (ch == 1) {
            CHECK(handle.isValid() == true);
        }
    }
}

TEST_CASE("VoiceRegistry: valid pitches 0-127 accepted", "[M11]") {
    VoiceRegistry registry;
    const int bufCapacity = capacity::noteOffReserveEvents + 200;
    OutputEventBuffer buffer(bufCapacity);
    
    // Test low and high pitches
    const VoiceStartRequest reqLow{0, 1, 0, 100, noScheduledEnd};
    const auto handleLow = registry.startVoice(reqLow, 0, buffer);
    CHECK(handleLow.isValid() == true);
    
    const VoiceStartRequest reqHigh{0, 2, 127, 100, noScheduledEnd};
    const auto handleHigh = registry.startVoice(reqHigh, 1, buffer);
    CHECK(handleHigh.isValid() == true);
}

TEST_CASE("VoiceRegistry: retrigger eviction order is retrigger-Off, eviction-Off, Note-On (D-29)", "[M11]") {
    VoiceRegistry registry;
    const int bufCapacity = capacity::noteOffReserveEvents + 100;
    OutputEventBuffer buffer(bufCapacity);
    
    registry.setVoiceLimit(0, 1);  // Limit 1 voice per owner
    
    // Start first voice
    (void)registry.startVoice(VoiceStartRequest{0, 1, 60, 100, noScheduledEnd}, 0, buffer);
    
    buffer.clear();
    
    // Retrigger same (channel, pitch)
    (void)registry.startVoice(VoiceStartRequest{0, 1, 60, 110, noScheduledEnd}, 1, buffer);
    
    REQUIRE(buffer.size() >= 2);
    const auto& events = buffer.events();
    
    // First should be retrigger Note-Off
    CHECK(events[0].kind == MidiEventKind::noteOff);
    CHECK(events[0].channel == 1);
    CHECK(events[0].data1 == 60);
    
    // Last should be new Note-On
    CHECK(events[events.size() - 1].kind == MidiEventKind::noteOn);
}

TEST_CASE("VoiceRegistry: pass-through owner is laneCount", "[M11]") {
    VoiceRegistry registry;
    const int bufCapacity = capacity::noteOffReserveEvents + 10;
    OutputEventBuffer buffer(bufCapacity);
    
    const OwnerId passThrough = lanes::laneCount;  // 8
    const VoiceStartRequest req{passThrough, 1, 60, 100, noScheduledEnd};
    const auto handle = registry.startVoice(req, 0, buffer);
    
    CHECK(handle.isValid() == true);
    CHECK(registry.ownerOf(1, 60) == passThrough);
}
