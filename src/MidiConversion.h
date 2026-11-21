#pragma once

#include "sequencing/MidiEvent.h"
#include "sequencing/Constants.h"

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <bitset>
#include <cstdint>
#include <span>

namespace phu::plugin {

// Classification of one host message (D-31): note events (Note-On incl. v=0, Note-Off) and CC64
// go to the core (CC64 additionally raw to the output); everything else (other CCs, pitch bend,
// aftertouch, program change, SysEx, realtime, ...) is raw-only.
enum class HostEventRoute : std::uint8_t { coreOnly, coreAndRaw, rawOnly };

[[nodiscard]] HostEventRoute routeOf(const juce::MidiMessageMetadata&) noexcept;
[[nodiscard]] sequencing::MidiEvent toCoreEvent(const juce::MidiMessageMetadata&, int sampleOffset) noexcept;

// Zero-sample deferral store (D-53). Fixed capacity, never allocates, cumulative across
// consecutive zero-sample host blocks; emptied only by the next non-empty host block via forEach
// + clear(). Entries keep input order. Release-class events (Note-Off, Note-On v=0, CC64 < 64,
// CC120, CC123) subsume any earlier deferred release event with the same identity and are never
// dropped; other events are refused (and counted) when the store runs out of entries/arena space.
class DeferredInputStore {
public:
    void insert(const juce::MidiMessageMetadata&) noexcept;                        // [RT]
    [[nodiscard]] bool empty() const noexcept;
    template <typename Visitor>
    void forEach(Visitor&& visitor) const noexcept {                               // [RT] input order, offset 0
        for (std::size_t i = 0; i < entryCount_; ++i) {
            const auto& entry = entries_[i];
            if ((entry.flags & flagActive) == 0) {
                continue;
            }
            if ((entry.flags & flagRelease) != 0) {
                visitor(entry.inlineBytes, static_cast<int>(entry.size));
            } else {
                visitor(arena_.data() + entry.arenaOffset, static_cast<int>(entry.size));
            }
        }
    }
    void clear() noexcept;                                                         // [RT]
    [[nodiscard]] std::uint32_t droppedEventCount() const noexcept;                // evicted + refused

private:
    static constexpr std::uint8_t flagActive = 0x01;
    static constexpr std::uint8_t flagRelease = 0x02;

    struct Entry {
        std::uint32_t arenaOffset = 0;
        std::uint16_t size = 0;
        std::uint8_t inlineBytes[3] = {0, 0, 0};
        std::uint8_t flags = 0;
    };

    [[nodiscard]] bool findReusableSlot(std::size_t& outIndex) noexcept;

    std::array<Entry, sequencing::constants::capacity::deferredEventCapacity> entries_{};
    std::array<std::uint8_t, sequencing::constants::capacity::deferredArenaBytes> arena_{};
    std::size_t entryCount_ = 0;
    std::uint32_t arenaUsed_ = 0;
    std::uint32_t droppedCount_ = 0;
};

// Streams the host buffer (and any deferred events) in input order into fixed-size chunks
// (D-51). Only core-routed events (note, CC64) are delivered; offsets are clamped to
// [0, numSamples - 1]. A chunk is cut only between complete sample-offset groups; a group larger
// than inputEventsPerChunk is split (in input order) into pieces with moreInputAtSameOffset=true
// on all but the last piece. The last chunk ends at numSamples; chunks may carry zero events
// (pure time-advance), since the core must still be called to advance transport/lane timing even
// on host blocks with no core-routed input.
class ChunkedInputReader {
public:
    ChunkedInputReader(const DeferredInputStore& deferred, const juce::MidiBuffer& hostInput,
                       int numSamples, int forcedCutSample = -1) noexcept;           // [RT]

    struct ChunkDescriptor {
        int startSample = 0;
        int numSamples = 0;
        int eventCount = 0;
        bool moreInputAtSameOffset = false;
    };

    bool next(std::span<sequencing::MidiEvent, sequencing::constants::capacity::inputEventsPerChunk> chunk,
              ChunkDescriptor&) noexcept;                                          // [RT]

private:
    void ensureHostCandidate() noexcept;
    bool consumeNextDeferredCore(sequencing::MidiEvent& outEvent) noexcept;

    const DeferredInputStore* deferred_;
    const juce::MidiBuffer* hostInput_;
    int numSamples_;
    int forcedCutSample_;
    int position_ = 0;

    std::size_t deferredTotalCore_ = 0;
    std::size_t deferredCoreEmitted_ = 0;

    juce::MidiBufferIterator hostIt_;
    juce::MidiBufferIterator hostEnd_;
    bool hostCandidateValid_ = false;
    int hostCandidateOffset_ = 0;
    sequencing::MidiEvent hostCandidateEvent_{};
};

// Fixed-capacity output writer over the processor-owned scratch juce::MidiBuffer (D-45). Never
// allocates and never clears its target implicitly (the caller calls beginBlock() once per
// non-empty host block). Admission rules (D-30, D-45) keep the invariant free >= open * 9, where
// `open` is the number of (channel, pitch) pairs whose Note-On was written and whose Note-Off was
// not yet written, and `free` is derived from a fixed logical capacity (outputMidiReserveBytes,
// the only capacity BlockEngine ever reserves for this buffer -- see note in MidiConversion.cpp
// about why this writer tracks its own logical budget rather than querying juce::MidiBuffer's
// real (private) allocated capacity).
class OutputMidiWriter {
public:
    OutputMidiWriter(juce::MidiBuffer& scratch,
                     std::bitset<sequencing::constants::capacity::maximumSoundingVoices>& outputOpen) noexcept;

    void beginBlock() noexcept;                                                    // [RT] scratch.clear()
    bool appendRaw(const juce::MidiMessageMetadata&, int sampleOffset) noexcept;   // [RT]
    void appendCoreEvents(std::span<const sequencing::MidiEvent>, int offsetShift) noexcept; // [RT]
    [[nodiscard]] std::uint32_t droppedEventCount() const noexcept;

private:
    [[nodiscard]] std::uint32_t openCount() const noexcept;

    juce::MidiBuffer* scratch_;
    std::bitset<sequencing::constants::capacity::maximumSoundingVoices>* outputOpen_;
    std::uint32_t usedBytes_ = 0;
    std::uint32_t droppedCount_ = 0;
};

}  // namespace phu::plugin
