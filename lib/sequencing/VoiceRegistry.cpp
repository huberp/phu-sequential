#include "VoiceRegistry.h"
#include "OutputEventBuffer.h"

#include <utility>

namespace phu::sequencing {

VoiceRegistry::VoiceRegistry() noexcept : globalHead_(noSlot), globalTail_(noSlot), endHeapSize_(0),
                                          nextStartSequence_(0), nextVoiceId_(1), soundingCount_(0) {
    ownerHead_.fill(noSlot);
    ownerTail_.fill(noSlot);
    ownerCount_.fill(0);
    ownerLimit_.fill(0);
    heapPosition_.fill(noHeapPosition);
}

void VoiceRegistry::reset() noexcept {
    for (auto& slot : slots_) {
        slot.id = invalidVoiceId;
        slot.owner = 0;
        slot.endBeat = noScheduledEnd;
        slot.previousInOwner = 0;
        slot.nextInOwner = 0;
        slot.previousGlobal = 0;
        slot.nextGlobal = 0;
    }
    ownerHead_.fill(noSlot);
    ownerTail_.fill(noSlot);
    ownerCount_.fill(0);
    globalHead_ = noSlot;
    globalTail_ = noSlot;
    heapPosition_.fill(noHeapPosition);
    endHeapSize_ = 0;
    soundingCount_ = 0;
    // ownerLimit_, nextVoiceId_ and nextStartSequence_ are configuration/monotonic counters and
    // intentionally survive a reset (used only after a flush, §3.11 architecture.md).
}

void VoiceRegistry::setVoiceLimit(OwnerId owner, int maximumVoices) noexcept {
    if (!isValidOwner(owner)) {
        return;
    }
    ownerLimit_[owner] = maximumVoices;
}

bool VoiceRegistry::isValidChannel(int channel) noexcept {
    return channel >= constants::midi::firstChannel && channel <= constants::midi::lastChannel;
}

bool VoiceRegistry::isValidPitch(int pitch) noexcept {
    return pitch >= constants::midi::lowestPitch && pitch <= constants::midi::highestPitch;
}

bool VoiceRegistry::isValidOwner(OwnerId owner) const noexcept {
    return owner < static_cast<OwnerId>(constants::capacity::voiceOwnerCount);
}

std::uint16_t VoiceRegistry::slotIndex(int channel, int pitch) noexcept {
    return static_cast<std::uint16_t>((channel - 1) * constants::midi::pitchCount + pitch);
}

VoiceId VoiceRegistry::allocateVoiceId() noexcept {
    const VoiceId id = nextVoiceId_;
    ++nextVoiceId_;
    if (nextVoiceId_ == invalidVoiceId) {
        // Wrap skipping 0 (invalidVoiceId), per §3.11 architecture.md.
        nextVoiceId_ = 1;
    }
    return id;
}

void VoiceRegistry::unlinkOwner(std::uint16_t slot) noexcept {
    const OwnerId owner = slots_[slot].owner;
    const std::uint16_t prev = slots_[slot].previousInOwner;
    const std::uint16_t next = slots_[slot].nextInOwner;
    if (prev != noSlot) {
        slots_[prev].nextInOwner = next;
    } else {
        ownerHead_[owner] = next;
    }
    if (next != noSlot) {
        slots_[next].previousInOwner = prev;
    } else {
        ownerTail_[owner] = prev;
    }
    --ownerCount_[owner];
}

void VoiceRegistry::appendOwner(OwnerId owner, std::uint16_t slot) noexcept {
    slots_[slot].previousInOwner = ownerTail_[owner];
    slots_[slot].nextInOwner = noSlot;
    if (ownerTail_[owner] != noSlot) {
        slots_[ownerTail_[owner]].nextInOwner = slot;
    } else {
        ownerHead_[owner] = slot;
    }
    ownerTail_[owner] = slot;
    ++ownerCount_[owner];
}

void VoiceRegistry::unlinkGlobal(std::uint16_t slot) noexcept {
    const std::uint16_t prev = slots_[slot].previousGlobal;
    const std::uint16_t next = slots_[slot].nextGlobal;
    if (prev != noSlot) {
        slots_[prev].nextGlobal = next;
    } else {
        globalHead_ = next;
    }
    if (next != noSlot) {
        slots_[next].previousGlobal = prev;
    } else {
        globalTail_ = prev;
    }
}

void VoiceRegistry::appendGlobal(std::uint16_t slot) noexcept {
    slots_[slot].previousGlobal = globalTail_;
    slots_[slot].nextGlobal = noSlot;
    if (globalTail_ != noSlot) {
        slots_[globalTail_].nextGlobal = slot;
    } else {
        globalHead_ = slot;
    }
    globalTail_ = slot;
}

bool VoiceRegistry::heapLess(std::uint16_t a, std::uint16_t b) const noexcept {
    const Beats endA = slots_[a].endBeat;
    const Beats endB = slots_[b].endBeat;
    if (endA != endB) {
        return endA < endB;
    }
    return startSequence_[a] < startSequence_[b];
}

void VoiceRegistry::heapSwap(int i, int j) noexcept {
    std::swap(endHeap_[static_cast<std::size_t>(i)], endHeap_[static_cast<std::size_t>(j)]);
    heapPosition_[endHeap_[static_cast<std::size_t>(i)]] = static_cast<std::uint16_t>(i);
    heapPosition_[endHeap_[static_cast<std::size_t>(j)]] = static_cast<std::uint16_t>(j);
}

void VoiceRegistry::heapSiftUp(int pos) noexcept {
    while (pos > 0) {
        const int parent = (pos - 1) / 2;
        if (heapLess(endHeap_[static_cast<std::size_t>(pos)], endHeap_[static_cast<std::size_t>(parent)])) {
            heapSwap(pos, parent);
            pos = parent;
        } else {
            break;
        }
    }
}

void VoiceRegistry::heapSiftDown(int pos) noexcept {
    for (;;) {
        const int left = 2 * pos + 1;
        const int right = 2 * pos + 2;
        int smallest = pos;
        if (left < endHeapSize_ &&
            heapLess(endHeap_[static_cast<std::size_t>(left)], endHeap_[static_cast<std::size_t>(smallest)])) {
            smallest = left;
        }
        if (right < endHeapSize_ &&
            heapLess(endHeap_[static_cast<std::size_t>(right)], endHeap_[static_cast<std::size_t>(smallest)])) {
            smallest = right;
        }
        if (smallest == pos) {
            break;
        }
        heapSwap(pos, smallest);
        pos = smallest;
    }
}

void VoiceRegistry::heapInsert(std::uint16_t slot) noexcept {
    const int pos = endHeapSize_;
    ++endHeapSize_;
    endHeap_[static_cast<std::size_t>(pos)] = slot;
    heapPosition_[slot] = static_cast<std::uint16_t>(pos);
    heapSiftUp(pos);
}

void VoiceRegistry::heapRemove(std::uint16_t slot) noexcept {
    const std::uint16_t pos16 = heapPosition_[slot];
    if (pos16 == noHeapPosition) {
        return;
    }
    const int pos = static_cast<int>(pos16);
    const int lastIndex = endHeapSize_ - 1;
    heapSwap(pos, lastIndex);
    --endHeapSize_;
    heapPosition_[slot] = noHeapPosition;
    if (pos < endHeapSize_) {
        heapSiftDown(pos);
        heapSiftUp(pos);
    }
}

void VoiceRegistry::removeVoiceBookkeeping(std::uint16_t slot) noexcept {
    heapRemove(slot);
    unlinkOwner(slot);
    unlinkGlobal(slot);
    slots_[slot].id = invalidVoiceId;
    slots_[slot].endBeat = noScheduledEnd;
    --soundingCount_;
}

void VoiceRegistry::retriggerAndEvict(OwnerId owner, std::uint16_t slot, SampleOffset sampleOffset,
                                       OutputEventBuffer* buffer) noexcept {
    // (1) Retrigger: a voice already sounding on this (channel, pitch) is ended and ownership
    // transfers to the new owner (§9.1, D-29 step 1).
    if (slots_[slot].id != invalidVoiceId) {
        const int channel = slot / constants::midi::pitchCount + 1;
        const int pitch = slot % constants::midi::pitchCount;
        removeVoiceBookkeeping(slot);
        if (buffer != nullptr) {
            buffer->pushNoteOff(makeNoteOff(sampleOffset, channel, pitch));
        }
    }

    // (2) Per-owner voice limit eviction: if the owner is still at its limit, its oldest voice is
    // ended before the new one starts (§9.4, D-29 step 2).
    const int limit = ownerLimit_[owner];
    if (limit > 0 && ownerCount_[owner] >= limit) {
        const std::uint16_t oldest = ownerHead_[owner];
        if (oldest != noSlot) {
            const int channel = oldest / constants::midi::pitchCount + 1;
            const int pitch = oldest % constants::midi::pitchCount;
            removeVoiceBookkeeping(oldest);
            if (buffer != nullptr) {
                buffer->pushNoteOff(makeNoteOff(sampleOffset, channel, pitch));
            }
        }
    }
}

VoiceHandle VoiceRegistry::startVoice(const VoiceStartRequest& request, SampleOffset sampleOffset,
                                      OutputEventBuffer& buffer) noexcept {
    if (!isValidOwner(request.owner) || !isValidChannel(request.channel) || !isValidPitch(request.pitch)) {
        return VoiceHandle{invalidVoiceId, 0};
    }

    const std::uint16_t slot = slotIndex(request.channel, request.pitch);
    retriggerAndEvict(request.owner, slot, sampleOffset, &buffer);

    const MidiEvent noteOn = makeNoteOn(sampleOffset, request.channel, request.pitch, request.velocity);
    if (!buffer.tryPushNoteOn(noteOn)) {
        return VoiceHandle{invalidVoiceId, 0};
    }

    const VoiceId id = allocateVoiceId();
    slots_[slot].id = id;
    slots_[slot].owner = request.owner;
    slots_[slot].endBeat = request.scheduledEndBeat;
    startSequence_[slot] = nextStartSequence_;
    ++nextStartSequence_;

    appendOwner(request.owner, slot);
    appendGlobal(slot);
    ++soundingCount_;

    if (request.scheduledEndBeat != noScheduledEnd) {
        heapInsert(slot);
    }

    return VoiceHandle{id, slot};
}

VoiceHandle VoiceRegistry::adoptVoice(const VoiceStartRequest& request) noexcept {
    if (!isValidOwner(request.owner) || !isValidChannel(request.channel) || !isValidPitch(request.pitch)) {
        return VoiceHandle{invalidVoiceId, 0};
    }

    const std::uint16_t slot = slotIndex(request.channel, request.pitch);
    retriggerAndEvict(request.owner, slot, 0, nullptr);

    const VoiceId id = allocateVoiceId();
    slots_[slot].id = id;
    slots_[slot].owner = request.owner;
    slots_[slot].endBeat = request.scheduledEndBeat;
    startSequence_[slot] = nextStartSequence_;
    ++nextStartSequence_;

    appendOwner(request.owner, slot);
    appendGlobal(slot);
    ++soundingCount_;

    if (request.scheduledEndBeat != noScheduledEnd) {
        heapInsert(slot);
    }

    return VoiceHandle{id, slot};
}

bool VoiceRegistry::endVoice(VoiceHandle handle, SampleOffset sampleOffset, OutputEventBuffer& buffer) noexcept {
    if (!handle.isValid() || handle.slot >= slots_.size() || slots_[handle.slot].id != handle.id) {
        // Stale handle (superseded by a retrigger/eviction) or never registered: suppressed (§9.1).
        return false;
    }

    const int channel = handle.slot / constants::midi::pitchCount + 1;
    const int pitch = handle.slot % constants::midi::pitchCount;
    removeVoiceBookkeeping(handle.slot);
    buffer.pushNoteOff(makeNoteOff(sampleOffset, channel, pitch));
    return true;
}

int VoiceRegistry::releaseOwner(OwnerId owner, SampleOffset sampleOffset, OutputEventBuffer& buffer) noexcept {
    if (!isValidOwner(owner)) {
        return 0;
    }
    int count = 0;
    while (ownerHead_[owner] != noSlot) {
        const std::uint16_t slot = ownerHead_[owner];
        const int channel = slot / constants::midi::pitchCount + 1;
        const int pitch = slot % constants::midi::pitchCount;
        removeVoiceBookkeeping(slot);
        buffer.pushNoteOff(makeNoteOff(sampleOffset, channel, pitch));
        ++count;
    }
    return count;
}

int VoiceRegistry::releaseAll(SampleOffset sampleOffset, OutputEventBuffer& buffer) noexcept {
    int count = 0;
    while (globalHead_ != noSlot) {
        const std::uint16_t slot = globalHead_;
        const int channel = slot / constants::midi::pitchCount + 1;
        const int pitch = slot % constants::midi::pitchCount;
        removeVoiceBookkeeping(slot);
        buffer.pushNoteOff(makeNoteOff(sampleOffset, channel, pitch));
        ++count;
    }
    return count;
}

std::optional<ScheduledVoiceEnd> VoiceRegistry::nextScheduledEnd() const noexcept {
    if (endHeapSize_ == 0) {
        return std::nullopt;
    }
    const std::uint16_t slot = endHeap_[0];
    return ScheduledVoiceEnd{slots_[slot].endBeat, VoiceHandle{slots_[slot].id, slot}};
}

bool VoiceRegistry::isSounding(VoiceHandle handle) const noexcept {
    return handle.isValid() && handle.slot < slots_.size() && slots_[handle.slot].id == handle.id;
}

int VoiceRegistry::soundingCount() const noexcept {
    return soundingCount_;
}

int VoiceRegistry::soundingCount(OwnerId owner) const noexcept {
    if (!isValidOwner(owner)) {
        return 0;
    }
    return ownerCount_[owner];
}

std::optional<OwnerId> VoiceRegistry::ownerOf(int channel, int pitch) const noexcept {
    if (!isValidChannel(channel) || !isValidPitch(pitch)) {
        return std::nullopt;
    }
    const std::uint16_t slot = slotIndex(channel, pitch);
    if (slots_[slot].id == invalidVoiceId) {
        return std::nullopt;
    }
    return slots_[slot].owner;
}

}  // namespace phu::sequencing
