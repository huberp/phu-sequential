#include "arpeggiator/ConfigurationBuilder.h"

#include <algorithm>
#include <cstdint>

namespace phu::arpeggiator {

namespace {

template <typename Enum>
Enum enumFromIndex(int index, int count) noexcept {
    return static_cast<Enum>(static_cast<std::uint8_t>(std::clamp(index, 0, count - 1)));
}

LaneParameters buildLane(const RawLaneParameterValues& raw) noexcept {
    LaneParameters lane;
    lane.mute = raw.mute != 0;
    lane.latch = raw.latch != 0;
    lane.inputMode = enumFromIndex<sequencing::InputMode>(raw.inputMode, sequencing::inputModeCount);
    lane.followIndex = raw.followIndex;
    lane.followChannel = raw.followChannel;
    lane.outputChannel = raw.outputChannel;
    lane.transpose = raw.transpose;
    lane.gatePercent = raw.gate;
    lane.timingSource = enumFromIndex<TimingSource>(raw.timingSource, timingSourceCount);
    lane.baseTimingIndex = raw.baseTiming;
    lane.subdivision = enumFromIndex<sequencing::Subdivision>(raw.subdivision, sequencing::subdivisionCount);
    lane.swingPercent = raw.swing;
    lane.stepCount = raw.stepCount;
    lane.velocityMode = enumFromIndex<sequencing::VelocityMode>(raw.velocityMode, sequencing::velocityModeCount);
    return clampLaneParameters(lane);
}

} // namespace

SequencerConfiguration buildConfiguration(const ParameterSnapshot& snapshot) noexcept {
    SequencerConfiguration config;
    config.global.thru = snapshot.thru != 0;
    config.global.consume = snapshot.consume != 0;
    config.global.chordInputChannel = snapshot.chordInputChannel;
    for (std::size_t i = 0; i < config.lanes.size(); ++i) {
        config.lanes[i] = buildLane(snapshot.lanes[i]);
    }
    return clampConfiguration(config);
}

} // namespace phu::arpeggiator
