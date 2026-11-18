#pragma once

#include "arpeggiator/LaneCommand.h"
#include "arpeggiator/LaneConfiguration.h"
#include "sequencing/NoteValues.h"
#include "sequencing/sources/SourceSelection.h"
#include "sequencing/velocity/VelocityMode.h"

#include <cstdint>
#include <functional>
#include <string>

namespace phu::arpeggiator {

// Shared implementation: a named command whose change is a function of the lane (sec. 24.3). Every command
// clamps its inputs to the spec ranges and keeps invariant I-1 (steps beyond the step count are default).
class FunctionLaneCommand : public LaneCommand {
public:
    FunctionLaneCommand(LaneStateAccess& access, int laneIndex, std::string name,
                        std::function<void(LaneState&)> change);

    [[nodiscard]] std::string getName() const override { return name_; }

protected:
    void doExecute(LaneState& lane) override;

private:
    std::string name_;
    std::function<void(LaneState&)> change_;
};

#define PHU_LANE_COMMAND(Name, ...)                                                    \
    class Name final : public FunctionLaneCommand {                                    \
    public:                                                                            \
        Name(LaneStateAccess& access, int laneIndex __VA_OPT__(, ) __VA_ARGS__);       \
    }

// Step manipulation (sec. 24.2.1); all act on the lane's active step count.
PHU_LANE_COMMAND(ReverseStepsCommand);
PHU_LANE_COMMAND(RotateStepsCommand, int amount);  // positive = towards higher steps
PHU_LANE_COMMAND(InvertStepsCommand);

// Pattern generation (sec. 24.2.2); deterministic, the caller supplies the seed (D-57).
PHU_LANE_COMMAND(FillRandomCommand, int probabilityPercent, std::uint32_t seed);
PHU_LANE_COMMAND(ArithmeticSequenceCommand, int start, int difference, int count);  // active velocity mode
PHU_LANE_COMMAND(EuclideanPatternCommand, int hits, int steps, int rotation);

// Step editing (sec. 24.2.3); out-of-range step indices leave the lane unchanged.
PHU_LANE_COMMAND(SetStepOnCommand, int stepIndex, bool on);
PHU_LANE_COMMAND(SetStepVelocityCommand, int stepIndex, int value);  // active velocity mode
PHU_LANE_COMMAND(ClearLaneCommand);
PHU_LANE_COMMAND(FillLaneCommand);

// Configuration (sec. 24.2.4).
PHU_LANE_COMMAND(SetInputModeCommand, sequencing::InputMode mode);
PHU_LANE_COMMAND(SetFollowIndexCommand, int index);
PHU_LANE_COMMAND(SetFollowChannelCommand, int channel);
PHU_LANE_COMMAND(SetOutputChannelCommand, int channel);
PHU_LANE_COMMAND(SetTransposeCommand, int semitones);
PHU_LANE_COMMAND(SetGateCommand, int percent);
PHU_LANE_COMMAND(SetVelocityModeCommand, sequencing::VelocityMode mode);
PHU_LANE_COMMAND(SetStepCountCommand, int count);
PHU_LANE_COMMAND(SetTimingSourceCommand, TimingSource source);
PHU_LANE_COMMAND(SetBaseTimingCommand, int baseTimingIndex, sequencing::Subdivision subdivision, int swingPercent);
PHU_LANE_COMMAND(SetMuteCommand, bool mute);
PHU_LANE_COMMAND(SetLatchCommand, bool latch);
PHU_LANE_COMMAND(SetLaneColorCommand, LaneColor color);
PHU_LANE_COMMAND(SetTextGridCommand, std::string text);  // text always stored; applied only if valid (sec. 5.3)

#undef PHU_LANE_COMMAND

}  // namespace phu::arpeggiator
