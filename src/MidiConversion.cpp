#include "MidiConversion.h"
#include "sequencing/MidiEvent.h"
#include "sequencing/Constants.h"

#include <algorithm>
#include <cstring>

namespace phu::plugin {

namespace {

using sequencing::MidiEvent;
using sequencing::MidiEventKind;
namespace capacity = sequencing::constants::capacity;

// Release-class classification shared by DeferredInputStore (D-53) and OutputMidiWriter (D-45):
// Note-Off, Note-On v=0, CC64 < 64, CC120, CC123.
[[nodiscard]] bool isReleaseMessageBytes(const std::uint8_t* data, int size) noexcept {
    if (data == nullptr || size < 2) {
        return false;
    }
    const int status = data[0] & 0xF0;
    if (status == 0x80) {
        return true;  // Note-Off
    }
    if (status == 0x90 && size >= 3 && data[2] == 0) {
        return true;  // Note-On velocity 0
    }
    if (status == 0xB0 && size >= 3) {
        const int controller = data[1];
        if (controller == sequencing::constants::midi::sustainPedalController &&
            data[2] < sequencing::constants::midi::sustainPedalOnThreshold) {
            return true;  // CC64 < 64
        }
        if (controller == 120 || controller == 123) {
            return true;  // CC120 / CC123
        }
    }
    return false;
}

// Identity used for release subsumption: Note-Off/Note-On v=0 share an id by (channel, pitch);
// CC64/CC120/CC123 are identified by (channel, controller).
[[nodiscard]] std::uint32_t releaseIdentity(const std::uint8_t* data, int size) noexcept {
    const int channel = data[0] & 0x0F;
    const int status = data[0] & 0xF0;
    if (status == 0x80 || status == 0x90) {
        const int pitch = size >= 2 ? data[1] : 0;
        return (1u << 16) | (static_cast<std::uint32_t>(channel) << 8) | static_cast<std::uint32_t>(pitch);
    }
    const int controller = size >= 2 ? data[1] : 0;
    return (2u << 16) | (static_cast<std::uint32_t>(channel) << 8) | static_cast<std::uint32_t>(controller);
}

}  // namespace

[[nodiscard]] HostEventRoute routeOf(const juce::MidiMessageMetadata& metadata) noexcept {
    const auto msg = metadata.getMessage();

    if (msg.isNoteOn() || msg.isNoteOff()) {
        return HostEventRoute::coreOnly;
    }

    if (msg.isController()) {
        const int controller = msg.getControllerNumber();
        if (controller == sequencing::constants::midi::sustainPedalController) {
            return HostEventRoute::coreAndRaw;
        }
        return HostEventRoute::rawOnly;
    }

    return HostEventRoute::rawOnly;
}

[[nodiscard]] sequencing::MidiEvent toCoreEvent(const juce::MidiMessageMetadata& metadata, int sampleOffset) noexcept {
    const auto msg = metadata.getMessage();
    const int channel = msg.getChannel();

    if (msg.isNoteOn()) {
        return sequencing::makeNoteOn(sampleOffset, channel, msg.getNoteNumber(), msg.getVelocity());
    }

    if (msg.isNoteOff()) {
        return sequencing::makeNoteOff(sampleOffset, channel, msg.getNoteNumber());
    }

    if (msg.isController() && msg.getControllerNumber() == sequencing::constants::midi::sustainPedalController) {
        MidiEvent event;
        event.sampleOffset = sampleOffset;
        event.kind = MidiEventKind::controlChange;
        event.channel = static_cast<std::uint8_t>(channel);
        event.data1 = static_cast<std::uint8_t>(msg.getControllerNumber());
        event.data2 = static_cast<std::uint8_t>(msg.getControllerValue());
        return event;
    }

    return MidiEvent{};
}

// --- DeferredInputStore -----------------------------------------------------------------------

bool DeferredInputStore::findReusableSlot(std::size_t& outIndex) noexcept {
    for (std::size_t i = 0; i < entryCount_; ++i) {
        if ((entries_[i].flags & flagActive) == 0) {
            outIndex = i;
            return true;
        }
    }
    if (entryCount_ < entries_.size()) {
        outIndex = entryCount_++;
        return true;
    }
    return false;
}

void DeferredInputStore::insert(const juce::MidiMessageMetadata& metadata) noexcept {
    const std::uint8_t* data = metadata.data;
    const int size = metadata.numBytes;
    if (data == nullptr || size <= 0) {
        return;
    }

    if (isReleaseMessageBytes(data, size)) {
        const auto id = releaseIdentity(data, size);

        // Subsume any earlier deferred release event with the same identity.
        for (std::size_t i = 0; i < entryCount_; ++i) {
            auto& existing = entries_[i];
            if ((existing.flags & flagActive) != 0 && (existing.flags & flagRelease) != 0 &&
                releaseIdentity(existing.inlineBytes, existing.size) == id) {
                existing.flags &= static_cast<std::uint8_t>(~flagActive);
                break;
            }
        }

        std::size_t index = 0;
        if (!findReusableSlot(index)) {
            // Store is full: evict the oldest non-release entry (always present per the capacity
            // invariant: release entries <= 2096 < deferredEventCapacity).
            bool evicted = false;
            for (std::size_t i = 0; i < entryCount_; ++i) {
                auto& existing = entries_[i];
                if ((existing.flags & flagActive) != 0 && (existing.flags & flagRelease) == 0) {
                    existing.flags &= static_cast<std::uint8_t>(~flagActive);
                    ++droppedCount_;
                    index = i;
                    evicted = true;
                    break;
                }
            }
            if (!evicted) {
                return;  // should not happen per the capacity invariant
            }
        }

        auto& slot = entries_[index];
        slot.size = static_cast<std::uint16_t>(size);
        for (int b = 0; b < size && b < 3; ++b) {
            slot.inlineBytes[b] = data[b];
        }
        slot.flags = flagActive | flagRelease;
        return;
    }

    // Non-release event: needs a free slot and arena room; refused (and counted) otherwise.
    std::size_t index = 0;
    if (!findReusableSlot(index) || arena_.size() - arenaUsed_ < static_cast<std::uint32_t>(size)) {
        ++droppedCount_;
        return;
    }

    auto& slot = entries_[index];
    slot.arenaOffset = arenaUsed_;
    slot.size = static_cast<std::uint16_t>(size);
    std::memcpy(arena_.data() + arenaUsed_, data, static_cast<std::size_t>(size));
    arenaUsed_ += static_cast<std::uint32_t>(size);
    slot.flags = flagActive;
}

bool DeferredInputStore::empty() const noexcept {
    for (std::size_t i = 0; i < entryCount_; ++i) {
        if ((entries_[i].flags & flagActive) != 0) {
            return false;
        }
    }
    return true;
}

void DeferredInputStore::clear() noexcept {
    entryCount_ = 0;
    arenaUsed_ = 0;
    droppedCount_ = 0;
}

std::uint32_t DeferredInputStore::droppedEventCount() const noexcept {
    return droppedCount_;
}

// --- ChunkedInputReader ------------------------------------------------------------------------

ChunkedInputReader::ChunkedInputReader(const DeferredInputStore& deferred, const juce::MidiBuffer& hostInput,
                                       int numSamples, int forcedCutSample) noexcept
    : deferred_(&deferred), hostInput_(&hostInput), numSamples_(numSamples), forcedCutSample_(forcedCutSample),
      hostIt_(hostInput.begin()), hostEnd_(hostInput.end()) {
    deferred.forEach([this](const std::uint8_t* data, int size) {
        const juce::MidiMessageMetadata md(data, size, 0);
        if (routeOf(md) != HostEventRoute::rawOnly) {
            ++deferredTotalCore_;
        }
    });
}

void ChunkedInputReader::ensureHostCandidate() noexcept {
    if (hostCandidateValid_) {
        return;
    }
    while (hostIt_ != hostEnd_) {
        const auto metadata = *hostIt_;
        ++hostIt_;
        if (routeOf(metadata) == HostEventRoute::rawOnly) {
            continue;
        }
        int offset = metadata.samplePosition;
        offset = std::clamp(offset, 0, numSamples_ - 1);
        hostCandidateOffset_ = offset;
        hostCandidateEvent_ = toCoreEvent(metadata, offset);
        hostCandidateValid_ = true;
        return;
    }
}

bool ChunkedInputReader::consumeNextDeferredCore(sequencing::MidiEvent& outEvent) noexcept {
    std::size_t seen = 0;
    bool found = false;
    deferred_->forEach([&](const std::uint8_t* data, int size) {
        if (found) {
            return;
        }
        const juce::MidiMessageMetadata md(data, size, 0);
        if (routeOf(md) == HostEventRoute::rawOnly) {
            return;
        }
        if (seen < deferredCoreEmitted_) {
            ++seen;
            return;
        }
        outEvent = toCoreEvent(md, 0);
        found = true;
    });
    if (found) {
        ++deferredCoreEmitted_;
    }
    return found;
}

bool ChunkedInputReader::next(std::span<sequencing::MidiEvent, capacity::inputEventsPerChunk> chunk,
                              ChunkDescriptor& descriptor) noexcept {
    if (position_ >= numSamples_) {
        return false;
    }

    const int startSample = position_;
    int hardEnd = std::min(startSample + static_cast<int>(capacity::samplesPerChunk), numSamples_);
    if (forcedCutSample_ > startSample && forcedCutSample_ < hardEnd) {
        hardEnd = forcedCutSample_;  // D-59: host loop wrap starts a new chunk
    }
    int eventCount = 0;
    int groupOffset = -1;
    bool moreAtSameOffset = false;
    int endPos = hardEnd;

    while (true) {
        ensureHostCandidate();
        const bool haveDeferred = deferredCoreEmitted_ < deferredTotalCore_;
        const bool haveHost = hostCandidateValid_;
        if (!haveDeferred && !haveHost) {
            break;
        }

        int candidateOffset = haveDeferred ? 0 : hostCandidateOffset_;
        if (candidateOffset < startSample) {
            candidateOffset = startSample;
        }
        if (candidateOffset >= hardEnd) {
            break;  // belongs to a later chunk
        }

        if (groupOffset == -1) {
            groupOffset = candidateOffset;
        }
        if (candidateOffset != groupOffset) {
            if (eventCount > 0) {
                endPos = candidateOffset;
                break;
            }
            groupOffset = candidateOffset;
        }

        if (eventCount == static_cast<int>(capacity::inputEventsPerChunk)) {
            moreAtSameOffset = true;
            endPos = groupOffset;
            break;
        }

        sequencing::MidiEvent event{};
        if (haveDeferred) {
            consumeNextDeferredCore(event);
        } else {
            event = hostCandidateEvent_;
            hostCandidateValid_ = false;
        }
        event.sampleOffset = candidateOffset - startSample;
        chunk[static_cast<std::size_t>(eventCount)] = event;
        ++eventCount;
    }

    descriptor.startSample = startSample;
    descriptor.numSamples = endPos - startSample;
    descriptor.eventCount = eventCount;
    descriptor.moreInputAtSameOffset = moreAtSameOffset;

    position_ = moreAtSameOffset ? startSample : endPos;
    return true;
}

// --- OutputMidiWriter --------------------------------------------------------------------------
//
// D-45 defines the byte budget via `C = target.data.getNumAllocated()`, i.e. the real allocated
// capacity of the scratch juce::MidiBuffer. juce::Array<uint8> (the storage behind MidiBuffer, in
// the JUCE version vendored here) does not expose its allocated capacity through any public API,
// so there is no way for this class to read that value directly. Since BlockEngine is the only
// owner of outputScratch_ and always reserves exactly outputMidiReserveBytes in prepare() (and
// never swaps or shrinks it), this writer instead tracks admission against that well-known fixed
// logical capacity and its own running `usedBytes_` counter (reset implicitly because a fresh
// OutputMidiWriter is constructed for every host block). This is a deliberate, documented
// deviation from the literal API call in the architecture text while preserving its behavior.

namespace {
constexpr std::uint32_t outputCapacityBytes = capacity::outputMidiReserveBytes;
constexpr std::uint32_t noteOffReserveBytes = static_cast<std::uint32_t>(capacity::noteOffReserveBytes);
constexpr std::uint32_t rawReleaseReserveBytes = static_cast<std::uint32_t>(capacity::rawReleaseReserveBytes);
constexpr std::uint32_t midiEventBytes = static_cast<std::uint32_t>(capacity::midiEventBytes);

[[nodiscard]] std::uint32_t eventByteCost(int numBytes) noexcept {
    return static_cast<std::uint32_t>(numBytes) + 4u + 2u;
}

[[nodiscard]] int voiceIndex(int channel, int pitch) noexcept {
    return (channel - 1) * sequencing::constants::midi::pitchCount + pitch;
}
}  // namespace

OutputMidiWriter::OutputMidiWriter(juce::MidiBuffer& scratch,
                                   std::bitset<capacity::maximumSoundingVoices>& outputOpen) noexcept
    : scratch_(&scratch), outputOpen_(&outputOpen) {
}

void OutputMidiWriter::beginBlock() noexcept {
    scratch_->clear();
    usedBytes_ = 0;
}

std::uint32_t OutputMidiWriter::openCount() const noexcept {
    return static_cast<std::uint32_t>(outputOpen_->count());
}

bool OutputMidiWriter::appendRaw(const juce::MidiMessageMetadata& metadata, int sampleOffset) noexcept {
    const std::uint32_t bytes = eventByteCost(metadata.numBytes);
    const std::uint32_t free = outputCapacityBytes - usedBytes_;

    const bool isReleaseClass = isReleaseMessageBytes(metadata.data, metadata.numBytes);
    bool admitted = false;
    if (isReleaseClass) {
        admitted = (free >= midiEventBytes) && (free - midiEventBytes >= openCount() * midiEventBytes);
    } else {
        admitted = (free >= bytes) && (free - bytes >= noteOffReserveBytes + rawReleaseReserveBytes);
    }

    if (!admitted) {
        ++droppedCount_;
        return false;
    }

    scratch_->addEvent(metadata.data, metadata.numBytes, sampleOffset);
    usedBytes_ += bytes;
    return true;
}

void OutputMidiWriter::appendCoreEvents(std::span<const sequencing::MidiEvent> events, int offsetShift) noexcept {
    for (const auto& event : events) {
        const int sampleOffset = event.sampleOffset + offsetShift;
        const int idx = voiceIndex(event.channel, event.data1);
        const bool isOpen = idx >= 0 && idx < capacity::maximumSoundingVoices && outputOpen_->test(static_cast<std::size_t>(idx));

        if (event.kind == MidiEventKind::noteOff || (event.kind == MidiEventKind::noteOn && event.data2 == 0)) {
            if (!isOpen) {
                continue;  // suppressed: its Note-On was refused earlier; nothing sounds
            }
            juce::uint8 bytes[3] = {static_cast<juce::uint8>(0x80 | ((event.channel - 1) & 0x0F)),
                                    static_cast<juce::uint8>(event.data1), static_cast<juce::uint8>(event.data2)};
            scratch_->addEvent(bytes, 3, sampleOffset);
            usedBytes_ += midiEventBytes;
            if (idx >= 0 && idx < capacity::maximumSoundingVoices) {
                outputOpen_->reset(static_cast<std::size_t>(idx));
            }
            continue;
        }

        if (event.kind == MidiEventKind::noteOn) {
            const std::uint32_t free = outputCapacityBytes - usedBytes_;
            if (free < midiEventBytes || free - midiEventBytes < noteOffReserveBytes) {
                ++droppedCount_;
                continue;
            }
            juce::uint8 bytes[3] = {static_cast<juce::uint8>(0x90 | ((event.channel - 1) & 0x0F)),
                                    static_cast<juce::uint8>(event.data1), static_cast<juce::uint8>(event.data2)};
            scratch_->addEvent(bytes, 3, sampleOffset);
            usedBytes_ += midiEventBytes;
            if (idx >= 0 && idx < capacity::maximumSoundingVoices) {
                outputOpen_->set(static_cast<std::size_t>(idx));
            }
        }
        // Other event kinds never reach the core output (the core only ever emits note events).
    }
}

std::uint32_t OutputMidiWriter::droppedEventCount() const noexcept {
    return droppedCount_;
}

}  // namespace phu::plugin
