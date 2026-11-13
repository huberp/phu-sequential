#include "PassThroughFilter.h"
#include "sequencing/Constants.h"

namespace phu::arpeggiator {

bool PassThroughPolicy::passesNoteOn(int channel) const noexcept {
    if (!thru) {
        return false;
    }
    return !consume || !followedChannels[channel - 1];
}

PassThroughPolicy makePassThroughPolicy(
    const GlobalParameters& global,
    std::span<const LaneParameters, sequencing::constants::lanes::laneCount> lanes) noexcept {
    using namespace sequencing::constants;
    
    PassThroughPolicy policy;
    policy.thru = global.thru;
    policy.consume = global.consume;
    
    if (policy.consume) {
        policy.followedChannels[global.chordInputChannel - 1] = true;
        
        for (const auto& lane : lanes) {
            if (lane.inputMode == sequencing::InputMode::channel) {
                policy.followedChannels[lane.followChannel - 1] = true;
            }
        }
    }
    
    return policy;
}

void PassThroughFilter::reset() noexcept {
    passedVoiceByKey_ = {};
    rawHeld_ = {};
    rawVelocity_ = {};
}

void PassThroughFilter::process(const sequencing::MidiEvent& event, const PassThroughPolicy& policy,
                               sequencing::VoiceRegistry& voices, sequencing::OutputEventBuffer& output) noexcept {
    if (event.kind == sequencing::MidiEventKind::noteOn && event.isNoteOnWithVelocity()) {
        if (policy.passesNoteOn(event.channel)) {
            sequencing::VoiceStartRequest request{
                passThroughOwner,
                event.channel,
                event.data1,
                event.data2,
                sequencing::noScheduledEnd
            };
            auto handle = voices.startVoice(request, event.sampleOffset, output);
            passedVoiceByKey_[event.channel - 1][event.data1] = handle;
        }
        return;
    }
    
    if (event.kind == sequencing::MidiEventKind::noteOff || 
        (event.kind == sequencing::MidiEventKind::noteOn && event.data2 == 0)) {
        auto handle = passedVoiceByKey_[event.channel - 1][event.data1];
        if (handle.isValid()) {
            voices.endVoice(handle, event.sampleOffset, output);
            passedVoiceByKey_[event.channel - 1][event.data1] = sequencing::VoiceHandle{};
        }
        return;
    }
}

void PassThroughFilter::clearTracking() noexcept {
    passedVoiceByKey_ = {};
}

void PassThroughFilter::trackRawNote(const sequencing::MidiEvent& normalizedEvent) noexcept {
    int chIdx = normalizedEvent.channel - 1;
    int pitchIdx = normalizedEvent.data1;
    
    if (normalizedEvent.kind == sequencing::MidiEventKind::noteOn && normalizedEvent.isNoteOnWithVelocity()) {
        rawHeld_[chIdx][pitchIdx] = true;
        rawVelocity_[chIdx][pitchIdx] = normalizedEvent.data2;
    } else if (normalizedEvent.kind == sequencing::MidiEventKind::noteOff || 
               (normalizedEvent.kind == sequencing::MidiEventKind::noteOn && normalizedEvent.data2 == 0)) {
        rawHeld_[chIdx][pitchIdx] = false;
    }
}

void PassThroughFilter::adoptRawNotes(sequencing::VoiceRegistry& voices) noexcept {
    for (int ch = 0; ch < sequencing::constants::midi::channelCount; ++ch) {
        for (int pitch = 0; pitch < sequencing::constants::midi::pitchCount; ++pitch) {
            if (rawHeld_[ch][pitch]) {
                sequencing::VoiceStartRequest request{
                    passThroughOwner,
                    ch + 1,
                    pitch,
                    rawVelocity_[ch][pitch],
                    sequencing::noScheduledEnd
                };
                auto handle = voices.adoptVoice(request);
                passedVoiceByKey_[ch][pitch] = handle;
            }
        }
    }
}

}
