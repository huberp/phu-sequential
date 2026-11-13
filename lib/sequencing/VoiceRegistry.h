#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <limits>

#include "MusicalTime.h"
#include "MidiEvent.h"
#include "Constants.h"

namespace phu::sequencing {

using VoiceId = std::uint32_t;
inline constexpr VoiceId invalidVoiceId = 0;
using OwnerId = std::uint8_t;
inline constexpr Beats noScheduledEnd = std::numeric_limits<Beats>::infinity();

struct VoiceStartRequest {
    OwnerId owner;
    int channel;
    int pitch;
    int velocity;
    Beats scheduledEndBeat = noScheduledEnd;
};

struct VoiceHandle {
    VoiceId id = invalidVoiceId;
    std::uint16_t slot = 0;
    
    [[nodiscard]] constexpr bool isValid() const noexcept { return id != invalidVoiceId; }
};

struct ScheduledVoiceEnd {
    Beats beat;
    VoiceHandle voice;
};

class OutputEventBuffer;

class VoiceRegistry {
public:
    VoiceRegistry() noexcept;
    
    void reset() noexcept;
    void setVoiceLimit(OwnerId, int maximumVoices) noexcept;
    
    [[nodiscard]] VoiceHandle startVoice(const VoiceStartRequest&, SampleOffset, OutputEventBuffer&) noexcept;
    [[nodiscard]] VoiceHandle adoptVoice(const VoiceStartRequest&) noexcept;
    bool endVoice(VoiceHandle, SampleOffset, OutputEventBuffer&) noexcept;
    
    int releaseOwner(OwnerId, SampleOffset, OutputEventBuffer&) noexcept;
    int releaseAll(SampleOffset, OutputEventBuffer&) noexcept;
    
    [[nodiscard]] std::optional<ScheduledVoiceEnd> nextScheduledEnd() const noexcept;
    [[nodiscard]] bool isSounding(VoiceHandle) const noexcept;
    [[nodiscard]] int soundingCount() const noexcept;
    [[nodiscard]] int soundingCount(OwnerId) const noexcept;
    [[nodiscard]] std::optional<OwnerId> ownerOf(int channel, int pitch) const noexcept;

private:
    struct Slot {
        VoiceId id = invalidVoiceId;
        OwnerId owner = 0;
        Beats endBeat = noScheduledEnd;
        std::uint16_t previousInOwner = 0;
        std::uint16_t nextInOwner = 0;
        std::uint16_t previousGlobal = 0;
        std::uint16_t nextGlobal = 0;
    };

    // Sentinel for "no slot"/"not in heap"; slot indices are 0..maximumSoundingVoices-1 (2047 max),
    // so the maximum std::uint16_t value is safely outside that range.
    static constexpr std::uint16_t noSlot = std::numeric_limits<std::uint16_t>::max();
    static constexpr std::uint16_t noHeapPosition = std::numeric_limits<std::uint16_t>::max();

    [[nodiscard]] static bool isValidChannel(int channel) noexcept;
    [[nodiscard]] static bool isValidPitch(int pitch) noexcept;
    [[nodiscard]] bool isValidOwner(OwnerId owner) const noexcept;
    [[nodiscard]] static std::uint16_t slotIndex(int channel, int pitch) noexcept;
    [[nodiscard]] VoiceId allocateVoiceId() noexcept;

    void unlinkOwner(std::uint16_t slot) noexcept;
    void appendOwner(OwnerId owner, std::uint16_t slot) noexcept;
    void unlinkGlobal(std::uint16_t slot) noexcept;
    void appendGlobal(std::uint16_t slot) noexcept;

    [[nodiscard]] bool heapLess(std::uint16_t a, std::uint16_t b) const noexcept;
    void heapSwap(int i, int j) noexcept;
    void heapSiftUp(int pos) noexcept;
    void heapSiftDown(int pos) noexcept;
    void heapInsert(std::uint16_t slot) noexcept;
    void heapRemove(std::uint16_t slot) noexcept;

    // Removes all registry bookkeeping (lists, heap, counts) for a sounding slot WITHOUT emitting
    // anything; the caller is responsible for emitting a Note-Off (or not) beforehand/afterwards.
    void removeVoiceBookkeeping(std::uint16_t slot) noexcept;

    // Shared retrigger + per-owner-limit-eviction bookkeeping for startVoice/adoptVoice (D-29).
    // Emits via `buffer` at `sampleOffset` iff buffer is non-null (startVoice); adoptVoice emits nothing.
    void retriggerAndEvict(OwnerId owner, std::uint16_t slot, SampleOffset sampleOffset,
                            OutputEventBuffer* buffer) noexcept;

    std::array<Slot, constants::capacity::maximumSoundingVoices> slots_;
    std::array<std::uint16_t, constants::capacity::voiceOwnerCount> ownerHead_;
    std::array<std::uint16_t, constants::capacity::voiceOwnerCount> ownerTail_;
    std::array<int, constants::capacity::voiceOwnerCount> ownerCount_;
    std::array<int, constants::capacity::voiceOwnerCount> ownerLimit_;
    std::uint16_t globalHead_ = 0;
    std::uint16_t globalTail_ = 0;
    std::array<std::uint16_t, constants::capacity::maximumSoundingVoices> endHeap_;
    std::array<std::uint16_t, constants::capacity::maximumSoundingVoices> heapPosition_;
    std::array<std::uint64_t, constants::capacity::maximumSoundingVoices> startSequence_;
    int endHeapSize_ = 0;
    std::uint64_t nextStartSequence_ = 0;
    VoiceId nextVoiceId_ = 1;
    int soundingCount_ = 0;
};

}  // namespace phu::sequencing
