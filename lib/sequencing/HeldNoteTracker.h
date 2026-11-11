#pragma once
#include <cstdint>
#include <array>
#include <optional>
#include <bitset>
#include "Constants.h"

namespace phu::sequencing {

struct HeldNote {
    std::uint8_t pitch;
    std::uint8_t velocity;
    friend constexpr bool operator==(const HeldNote&, const HeldNote&) noexcept = default;
};

class MidiEvent;

class HeldNoteTracker {
public:
    void reset() noexcept;
    bool apply(const MidiEvent& event) noexcept;
    [[nodiscard]] bool isHeld(int channel, int pitch) const noexcept;
    [[nodiscard]] int heldCount(int channel) const noexcept;
    [[nodiscard]] std::optional<HeldNote> heldNoteAtIndex(int channel, int sortedIndex) const noexcept;
    [[nodiscard]] std::optional<HeldNote> mostRecentNote(int channel) const noexcept;
    [[nodiscard]] bool isSustainPedalDown(int channel) const noexcept;

private:
    struct ChannelState {
        std::bitset<constants::midi::pitchCount> keyDown;
        std::bitset<constants::midi::pitchCount> sustained;
        std::array<std::uint8_t, constants::midi::pitchCount> velocity{};
        int mostRecentPitch = -1;
        bool pedalDown = false;
    };
    std::array<ChannelState, constants::midi::channelCount> channels_{};
};

}  // namespace phu::sequencing
