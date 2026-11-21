#include "HostTransport.h"

namespace phu::plugin {

[[nodiscard]] arpeggiator::TransportInfo readHostTransport(juce::AudioPlayHead* playhead) noexcept {
    if (playhead == nullptr) {
        return arpeggiator::TransportInfo{};
    }
    
    auto positionInfoOpt = playhead->getPosition();
    if (!positionInfoOpt) {
        return arpeggiator::TransportInfo{};
    }
    
    auto positionInfo = *positionInfoOpt;
    
    arpeggiator::TransportInfo info;
    info.isPlaying = positionInfo.getIsPlaying();
    
    if (auto ppq = positionInfo.getPpqPosition()) {
        info.hasMusicalPosition = true;
        info.playheadBeat = *ppq;
    }
    
    if (auto tempo = positionInfo.getBpm()) {
        info.tempoBeatsPerMinute = *tempo;
    }

    if (auto loop = positionInfo.getLoopPoints(); loop && positionInfo.getIsLooping()) {
        info.isLooping = true;
        info.loopStartBeat = loop->ppqStart;
        info.loopEndBeat = loop->ppqEnd;
    }
    
    return info;
}

}  // namespace phu::plugin
