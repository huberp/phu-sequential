#include "OutputEventBuffer.h"

namespace phu::sequencing {

OutputEventBuffer::OutputEventBuffer(std::size_t capacity) : storage_(capacity), used_(0), dropped_(0) {
}

void OutputEventBuffer::clear() noexcept {
    used_ = 0;
    dropped_ = 0;
}

bool OutputEventBuffer::pushNoteOff(const MidiEvent& event) noexcept {
    // Reserve partition (D-30): Note-Offs may always use the full buffer, including the
    // last noteOffReserveEvents slots that Note-Ons are forbidden from touching.
    if (used_ >= storage_.size()) {
        return false;
    }
    storage_[used_] = event;
    ++used_;
    return true;
}

bool OutputEventBuffer::tryPushNoteOn(const MidiEvent& event) noexcept {
    // Note-Ons are admitted only while at least noteOffReserveEvents slots stay free afterwards.
    const auto reserve = static_cast<std::size_t>(constants::capacity::noteOffReserveEvents);
    const std::size_t generalCapacity = storage_.size() > reserve ? storage_.size() - reserve : 0;
    if (used_ + 1 > generalCapacity) {
        ++dropped_;
        return false;
    }
    storage_[used_] = event;
    ++used_;
    return true;
}

std::span<const MidiEvent> OutputEventBuffer::events() const noexcept {
    return std::span<const MidiEvent>(storage_.data(), used_);
}

std::size_t OutputEventBuffer::size() const noexcept {
    return used_;
}

std::size_t OutputEventBuffer::capacity() const noexcept {
    return storage_.size();
}

std::uint32_t OutputEventBuffer::droppedEventCount() const noexcept {
    return dropped_;
}

}  // namespace phu::sequencing
