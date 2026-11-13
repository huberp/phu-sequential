#pragma once

#include <vector>
#include <span>
#include <cstddef>
#include <cstdint>

#include "Constants.h"
#include "MidiEvent.h"

namespace phu::sequencing {

class OutputEventBuffer {
public:
    explicit OutputEventBuffer(std::size_t capacity = constants::capacity::outputEventsPerBlock);
    
    void clear() noexcept;
    bool pushNoteOff(const MidiEvent&) noexcept;
    [[nodiscard]] bool tryPushNoteOn(const MidiEvent&) noexcept;
    [[nodiscard]] std::span<const MidiEvent> events() const noexcept;
    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] std::size_t capacity() const noexcept;
    [[nodiscard]] std::uint32_t droppedEventCount() const noexcept;

private:
    std::vector<MidiEvent> storage_;
    std::size_t used_ = 0;
    std::uint32_t dropped_ = 0;
};

}  // namespace phu::sequencing
