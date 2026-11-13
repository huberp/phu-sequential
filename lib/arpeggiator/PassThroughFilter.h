#pragma once

#include "sequencing/MidiEvent.h"
#include "sequencing/VoiceRegistry.h"
#include "sequencing/OutputEventBuffer.h"
#include "LaneConfiguration.h"
#include <bitset>
#include <array>
#include <cstdint>

namespace phu::arpeggiator {

struct PassThroughPolicy {
    bool thru = false;
    bool consume = true;
    std::bitset<sequencing::constants::midi::channelCount> followedChannels;

    [[nodiscard]] bool passesNoteOn(int channel) const noexcept;
};

[[nodiscard]] PassThroughPolicy makePassThroughPolicy(const GlobalParameters&,
                                                      std::span<const LaneParameters, sequencing::constants::lanes::laneCount>) noexcept;

inline constexpr sequencing::OwnerId passThroughOwner = static_cast<sequencing::OwnerId>(sequencing::constants::lanes::laneCount);

class PassThroughFilter {
public:
    void reset() noexcept;
    void process(const sequencing::MidiEvent& event, const PassThroughPolicy&,
                 sequencing::VoiceRegistry&, sequencing::OutputEventBuffer&) noexcept;
    void clearTracking() noexcept;
    void trackRawNote(const sequencing::MidiEvent& normalizedEvent) noexcept;
    void adoptRawNotes(sequencing::VoiceRegistry&) noexcept;

private:
    std::array<std::array<sequencing::VoiceHandle, sequencing::constants::midi::pitchCount>, 
               sequencing::constants::midi::channelCount> passedVoiceByKey_{};
    std::array<std::bitset<sequencing::constants::midi::pitchCount>, 
               sequencing::constants::midi::channelCount> rawHeld_{};
    std::array<std::array<std::uint8_t, sequencing::constants::midi::pitchCount>, 
               sequencing::constants::midi::channelCount> rawVelocity_{};
};

}
