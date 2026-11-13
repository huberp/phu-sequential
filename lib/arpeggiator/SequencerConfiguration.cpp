#include "SequencerConfiguration.h"
#include "arpeggiator/LaneConfiguration.h"
#include <algorithm>

namespace phu::arpeggiator {

SequencerConfiguration defaultSequencerConfiguration() noexcept {
    SequencerConfiguration config;
    config.global = {};
    for (int i = 0; i < sequencing::constants::lanes::laneCount; ++i) {
        config.lanes[i] = {};
    }
    return config;
}

SequencerConfiguration clampConfiguration(const SequencerConfiguration& config) noexcept {
    SequencerConfiguration clamped = config;
    clamped.global.chordInputChannel = std::clamp(config.global.chordInputChannel,
                                                    sequencing::constants::midi::firstChannel,
                                                    sequencing::constants::midi::lastChannel);
    for (int i = 0; i < sequencing::constants::lanes::laneCount; ++i) {
        clamped.lanes[i] = clampLaneParameters(config.lanes[i]);
    }
    return clamped;
}

}
