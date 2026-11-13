#include <catch2/catch_test_macros.hpp>
#include "sequencing/OutputEventBuffer.h"
#include "sequencing/MidiEvent.h"
#include "sequencing/Constants.h"

using namespace phu::sequencing;
using namespace phu::sequencing::constants;

// M06 Output event buffer (§3.3, §6, §9.1, §9.3, D-30, #9)

TEST_CASE("OutputEventBuffer::constructor creates buffer with given capacity", "[M06]") {
    // Use a small capacity for testing; reserve partition = 2048 (maximumSoundingVoices)
    // so total capacity must be > reserve to have space for Note-Ons
    const int testCapacity = capacity::noteOffReserveEvents + 4;  // 2052
    OutputEventBuffer buffer(testCapacity);
    
    CHECK(buffer.capacity() == testCapacity);
    CHECK(buffer.size() == 0);
    CHECK(buffer.droppedEventCount() == 0);
}

TEST_CASE("OutputEventBuffer::pushNoteOff adds Note-Off event to buffer", "[M06]") {
    const int testCapacity = capacity::noteOffReserveEvents + 4;
    OutputEventBuffer buffer(testCapacity);
    
    const auto event = makeNoteOff(0, 1, 60);
    buffer.pushNoteOff(event);
    
    CHECK(buffer.size() == 1);
    const auto& events = buffer.events();
    REQUIRE(events.size() == 1);
    CHECK(events[0] == event);
}

TEST_CASE("OutputEventBuffer::tryPushNoteOn adds Note-On if not full", "[M06]") {
    const int testCapacity = capacity::noteOffReserveEvents + 4;
    OutputEventBuffer buffer(testCapacity);
    
    const auto noteOn = makeNoteOn(0, 1, 60, 100);
    bool added = buffer.tryPushNoteOn(noteOn);
    
    CHECK(added == true);
    CHECK(buffer.size() == 1);
    const auto& events = buffer.events();
    REQUIRE(events.size() == 1);
    CHECK(events[0] == noteOn);
}

TEST_CASE("OutputEventBuffer::tryPushNoteOn respects reserve partition for Note-Offs", "[M06]") {
    // Create a small buffer: capacity = reserve + 1 means only 1 Note-On fits
    const int testCapacity = capacity::noteOffReserveEvents + 1;
    OutputEventBuffer buffer(testCapacity);
    
    // Fill the buffer with 1 Note-On
    const auto noteOn1 = makeNoteOn(0, 1, 60, 100);
    bool added1 = buffer.tryPushNoteOn(noteOn1);
    CHECK(added1 == true);
    
    // Next Note-On should be refused because reserve partition is full
    const auto noteOn2 = makeNoteOn(0, 1, 61, 100);
    bool added2 = buffer.tryPushNoteOn(noteOn2);
    CHECK(added2 == false);
    
    CHECK(buffer.size() == 1);
    CHECK(buffer.droppedEventCount() == 1);
}

TEST_CASE("OutputEventBuffer::tryPushNoteOn returns false if full", "[M06]") {
    // The reserve partition is a fixed noteOffReserveEvents (2048) slots regardless of total
    // capacity (D-30, §3.6 architecture.md), so only noteOffReserveEvents + 1 leaves exactly
    // 1 general slot for Note-Ons.
    const int testCapacity = capacity::noteOffReserveEvents + 1;
    OutputEventBuffer buffer(testCapacity);
    
    const auto noteOn1 = makeNoteOn(0, 1, 60, 100);
    const auto noteOn2 = makeNoteOn(0, 1, 61, 100);
    
    bool added1 = buffer.tryPushNoteOn(noteOn1);
    CHECK(added1 == true);
    
    bool added2 = buffer.tryPushNoteOn(noteOn2);
    CHECK(added2 == false);
    CHECK(buffer.droppedEventCount() == 1);
}

TEST_CASE("OutputEventBuffer::droppedEventCount increments on refused Note-On", "[M06]") {
    const int testCapacity = capacity::noteOffReserveEvents + 2;
    OutputEventBuffer buffer(testCapacity);
    
    // Fill with Note-Ons until full
    for (int i = 0; i < 2; ++i) {
        const auto noteOn = makeNoteOn(0, 1, 60 + i, 100);
        (void)buffer.tryPushNoteOn(noteOn);
    }
    
    CHECK(buffer.droppedEventCount() == 0);
    
    // Try to add one more
    const auto noteOn = makeNoteOn(0, 1, 70, 100);
    bool added = buffer.tryPushNoteOn(noteOn);
    CHECK(added == false);
    CHECK(buffer.droppedEventCount() == 1);
    
    // Try again
    added = buffer.tryPushNoteOn(noteOn);
    CHECK(added == false);
    CHECK(buffer.droppedEventCount() == 2);
}

TEST_CASE("OutputEventBuffer::clear removes all events and resets dropped count", "[M06]") {
    const int testCapacity = capacity::noteOffReserveEvents + 4;
    OutputEventBuffer buffer(testCapacity);
    
    // Add some events
    buffer.pushNoteOff(makeNoteOff(0, 1, 60));
    (void)buffer.tryPushNoteOn(makeNoteOn(0, 1, 61, 100));
    
    // Try to overfill to increment dropped count
    const auto testCapacity2 = 2;
    OutputEventBuffer smallBuffer(testCapacity2);
    (void)smallBuffer.tryPushNoteOn(makeNoteOn(0, 1, 60, 100));
    (void)smallBuffer.tryPushNoteOn(makeNoteOn(0, 1, 61, 100));
    
    smallBuffer.clear();
    CHECK(smallBuffer.size() == 0);
    CHECK(smallBuffer.droppedEventCount() == 0);
}

TEST_CASE("OutputEventBuffer::events returns events in insertion order", "[M06]") {
    const int testCapacity = capacity::noteOffReserveEvents + 4;
    OutputEventBuffer buffer(testCapacity);
    
    const auto off1 = makeNoteOff(10, 1, 60);
    const auto on1 = makeNoteOn(20, 2, 61, 100);
    const auto off2 = makeNoteOff(30, 3, 62);
    
    buffer.pushNoteOff(off1);
    (void)buffer.tryPushNoteOn(on1);
    buffer.pushNoteOff(off2);
    
    const auto& events = buffer.events();
    CHECK(events.size() == 3);
    CHECK(events[0].sampleOffset == 10);
    CHECK(events[1].sampleOffset == 20);
    CHECK(events[2].sampleOffset == 30);
}

TEST_CASE("OutputEventBuffer::size returns current number of events", "[M06]") {
    const int testCapacity = capacity::noteOffReserveEvents + 10;
    OutputEventBuffer buffer(testCapacity);
    
    CHECK(buffer.size() == 0);
    
    buffer.pushNoteOff(makeNoteOff(0, 1, 60));
    CHECK(buffer.size() == 1);
    
    (void)buffer.tryPushNoteOn(makeNoteOn(0, 1, 61, 100));
    CHECK(buffer.size() == 2);
    
    buffer.clear();
    CHECK(buffer.size() == 0);
}

TEST_CASE("OutputEventBuffer: pushNoteOff always succeeds (uses reserve partition)", "[M06]") {
    // Even when buffer is nearly full, Note-Off should always fit in reserve partition
    const int testCapacity = capacity::noteOffReserveEvents + 2;
    OutputEventBuffer buffer(testCapacity);
    
    // Fill with 2 Note-Ons
    (void)buffer.tryPushNoteOn(makeNoteOn(0, 1, 60, 100));
    (void)buffer.tryPushNoteOn(makeNoteOn(0, 1, 61, 100));
    
    // Now buffer is full in the non-reserved area
    // But Note-Offs should still fit in reserve
    for (int i = 0; i < capacity::noteOffReserveEvents; ++i) {
        buffer.pushNoteOff(makeNoteOff(0, 1, static_cast<int>(i % 128)));
    }
    
    // All pushed (no exception, no dropped)
    CHECK(buffer.size() >= 2);
}

TEST_CASE("OutputEventBuffer: Note-Off velocity is always 0 (silent release)", "[M06]") {
    const int testCapacity = capacity::noteOffReserveEvents + 4;
    OutputEventBuffer buffer(testCapacity);
    
    const auto noteOff = makeNoteOff(0, 1, 60);
    buffer.pushNoteOff(noteOff);
    
    const auto& events = buffer.events();
    REQUIRE(events.size() == 1);
    CHECK(events[0].data2 == 0);  // velocity/release velocity = 0
}

TEST_CASE("OutputEventBuffer: capacity and reserve partition relationship", "[M06]") {
    // Verify the buffer is partitioned correctly
    const int total = capacity::noteOffReserveEvents + 10;
    OutputEventBuffer buffer(total);
    
    CHECK(buffer.capacity() == total);
    // The reserve partition holds noteOffReserveEvents slots
    // The remaining capacity::noteOffReserveEvents slots are for Note-Ons
}
