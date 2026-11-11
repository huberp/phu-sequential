#pragma once
#include <cstdint>
#include <compare>

namespace phu::sequencing {

using SampleOffset = std::int32_t;

enum class MidiEventKind : std::uint8_t {
    noteOn,          // data1 = pitch, data2 = velocity (0 allowed on INPUT; normalized by consumers, §3.1)
    noteOff,         // data1 = pitch, data2 = release velocity
    controlChange    // data1 = controller, data2 = value (the arpeggiator core only receives CC64, D-31)
};

struct MidiEvent {
    SampleOffset sampleOffset = 0;
    MidiEventKind kind = MidiEventKind::noteOn;
    std::uint8_t channel = 1;        // 1..16
    std::uint8_t data1 = 0;
    std::uint8_t data2 = 0;

    [[nodiscard]] constexpr bool isNoteOnWithVelocity() const noexcept;
    [[nodiscard]] constexpr bool isNoteRelease() const noexcept;
    friend constexpr bool operator==(const MidiEvent&, const MidiEvent&) noexcept = default;
};

[[nodiscard]] constexpr MidiEvent makeNoteOn(SampleOffset, int channel, int pitch, int velocity) noexcept;
[[nodiscard]] constexpr MidiEvent makeNoteOff(SampleOffset, int channel, int pitch) noexcept;
[[nodiscard]] constexpr MidiEvent normalizeNoteEvent(const MidiEvent&) noexcept;

// Implementation (constexpr inline)
constexpr bool MidiEvent::isNoteOnWithVelocity() const noexcept {
    return kind == MidiEventKind::noteOn && data2 > 0;
}

constexpr bool MidiEvent::isNoteRelease() const noexcept {
    return kind == MidiEventKind::noteOff || (kind == MidiEventKind::noteOn && data2 == 0);
}

constexpr MidiEvent makeNoteOn(SampleOffset sampleOffset, int channel, int pitch, int velocity) noexcept {
    return MidiEvent{sampleOffset, MidiEventKind::noteOn, static_cast<std::uint8_t>(channel),
                     static_cast<std::uint8_t>(pitch), static_cast<std::uint8_t>(velocity)};
}

constexpr MidiEvent makeNoteOff(SampleOffset sampleOffset, int channel, int pitch) noexcept {
    return MidiEvent{sampleOffset, MidiEventKind::noteOff, static_cast<std::uint8_t>(channel),
                     static_cast<std::uint8_t>(pitch), 0};
}

constexpr MidiEvent normalizeNoteEvent(const MidiEvent& event) noexcept {
    if (event.kind == MidiEventKind::noteOn && event.data2 == 0) {
        return MidiEvent{event.sampleOffset, MidiEventKind::noteOff, event.channel, event.data1, 0};
    }
    return event;
}

}  // namespace phu::sequencing
