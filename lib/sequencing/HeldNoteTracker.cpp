#include "HeldNoteTracker.h"
#include "MidiEvent.h"
#include "Constants.h"

namespace phu::sequencing {

void HeldNoteTracker::reset() noexcept {
    for (auto& channel : channels_) {
        channel.keyDown.reset();
        channel.sustained.reset();
        channel.velocity.fill(0);
        channel.mostRecentPitch = -1;
        channel.pedalDown = false;
    }
}

bool HeldNoteTracker::apply(const MidiEvent& event) noexcept {
    // Validate channel (1-16 maps to 0-15)
    if (event.channel < constants::midi::firstChannel || event.channel > constants::midi::lastChannel) {
        return false;
    }

    const int channelIndex = event.channel - 1;
    auto& state = channels_[channelIndex];
    bool changed = false;

    if (event.kind == MidiEventKind::noteOn) {
        const int pitch = event.data1;
        const int velocity = event.data2;

        // Validate pitch
        if (pitch < constants::midi::lowestPitch || pitch > constants::midi::highestPitch) {
            return false;
        }

        if (velocity > 0) {
            // Note-On: mark as key down, store velocity, update most recent
            const bool wasHeld = state.keyDown[pitch] || state.sustained[pitch];
            const std::uint8_t oldVelocity = state.velocity[pitch];
            const int oldMostRecent = state.mostRecentPitch;
            
            state.keyDown[pitch] = true;
            state.sustained[pitch] = false;
            state.velocity[pitch] = velocity;
            state.mostRecentPitch = pitch;
            
            // Changed if: note wasn't held before, OR velocity changed, OR mostRecentPitch changed
            changed = !wasHeld || (oldVelocity != velocity) || (oldMostRecent != pitch);
        } else {
            // Note-On with velocity 0 acts as Note-Off
            const bool wasHeld = state.keyDown[pitch] || state.sustained[pitch];
            state.sustained[pitch] = state.pedalDown && wasHeld;
            state.keyDown[pitch] = false;
            // Clear mostRecentPitch only if it was tracking this pitch and it's not held anymore
            if (state.mostRecentPitch == pitch && !state.keyDown[pitch] && !state.sustained[pitch]) {
                state.mostRecentPitch = -1;
            }
            changed = wasHeld;
        }
    } else if (event.kind == MidiEventKind::noteOff) {
        const int pitch = event.data1;

        // Validate pitch
        if (pitch < constants::midi::lowestPitch || pitch > constants::midi::highestPitch) {
            return false;
        }

        const bool wasHeld = state.keyDown[pitch] || state.sustained[pitch];
        state.sustained[pitch] = state.pedalDown && wasHeld;
        state.keyDown[pitch] = false;
        // Clear mostRecentPitch only if it was tracking this pitch and it's not held anymore
        if (state.mostRecentPitch == pitch && !state.keyDown[pitch] && !state.sustained[pitch]) {
            state.mostRecentPitch = -1;
        }
        changed = wasHeld;
    } else if (event.kind == MidiEventKind::controlChange) {
        if (event.data1 == constants::midi::sustainPedalController) {
            const bool newPedalDown = event.data2 >= constants::midi::sustainPedalOnThreshold;
            if (newPedalDown != state.pedalDown) {
                state.pedalDown = newPedalDown;
                if (!newPedalDown) {
                    // Pedal up: clear sustained notes
                    state.sustained.reset();
                    changed = true;
                }
            }
        } else {
            // Other CC events: ignored
            return false;
        }
    } else {
        return false;
    }

    return changed;
}

bool HeldNoteTracker::isHeld(int channel, int pitch) const noexcept {
    // Validate inputs
    if (channel < constants::midi::firstChannel || channel > constants::midi::lastChannel) {
        return false;
    }
    if (pitch < constants::midi::lowestPitch || pitch > constants::midi::highestPitch) {
        return false;
    }

    const int channelIndex = channel - 1;
    const auto& state = channels_[channelIndex];
    return state.keyDown[pitch] || state.sustained[pitch];
}

int HeldNoteTracker::heldCount(int channel) const noexcept {
    // Validate channel
    if (channel < constants::midi::firstChannel || channel > constants::midi::lastChannel) {
        return 0;
    }

    const int channelIndex = channel - 1;
    const auto& state = channels_[channelIndex];
    return static_cast<int>((state.keyDown | state.sustained).count());
}

std::optional<HeldNote> HeldNoteTracker::heldNoteAtIndex(int channel, int sortedIndex) const noexcept {
    // Validate channel
    if (channel < constants::midi::firstChannel || channel > constants::midi::lastChannel) {
        return std::nullopt;
    }

    const int channelIndex = channel - 1;
    const auto& state = channels_[channelIndex];

    // Count held notes in ascending pitch order
    int count = 0;
    for (int pitch = constants::midi::lowestPitch; pitch <= constants::midi::highestPitch; ++pitch) {
        if (state.keyDown[pitch] || state.sustained[pitch]) {
            if (count == sortedIndex) {
                return HeldNote{static_cast<std::uint8_t>(pitch), state.velocity[pitch]};
            }
            ++count;
        }
    }

    return std::nullopt;
}

std::optional<HeldNote> HeldNoteTracker::mostRecentNote(int channel) const noexcept {
    // Validate channel
    if (channel < constants::midi::firstChannel || channel > constants::midi::lastChannel) {
        return std::nullopt;
    }

    const int channelIndex = channel - 1;
    const auto& state = channels_[channelIndex];

    if (state.mostRecentPitch < 0) {
        return std::nullopt;
    }

    // Still held: key down or sustained by the pedal (§3.1)
    if (state.keyDown[state.mostRecentPitch] || state.sustained[state.mostRecentPitch]) {
        return HeldNote{static_cast<std::uint8_t>(state.mostRecentPitch), state.velocity[state.mostRecentPitch]};
    }

    return std::nullopt;
}

bool HeldNoteTracker::isSustainPedalDown(int channel) const noexcept {
    // Validate channel
    if (channel < constants::midi::firstChannel || channel > constants::midi::lastChannel) {
        return false;
    }

    const int channelIndex = channel - 1;
    return channels_[channelIndex].pedalDown;
}

}  // namespace phu::sequencing
