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

// Step manipulation (sec. 24.2.1); all act on the lane's active step count.
class ReverseStepsCommand final : public FunctionLaneCommand {
public:
    ReverseStepsCommand(LaneStateAccess& access, int laneIndex);
};
class RotateStepsCommand final : public FunctionLaneCommand {
public:
    RotateStepsCommand(LaneStateAccess& access, int laneIndex, int amount);
};
class InvertStepsCommand final : public FunctionLaneCommand {
public:
    InvertStepsCommand(LaneStateAccess& access, int laneIndex);
};

// Pattern generation (sec. 24.2.2); deterministic, the caller supplies the seed (D-57).
class FillRandomCommand final : public FunctionLaneCommand {
public:
    FillRandomCommand(LaneStateAccess& access, int laneIndex, int probabilityPercent, std::uint32_t seed);
};
class ArithmeticSequenceCommand final : public FunctionLaneCommand {
public:
    ArithmeticSequenceCommand(LaneStateAccess& access, int laneIndex, int start, int difference, int count);
};
class EuclideanPatternCommand final : public FunctionLaneCommand {
public:
    EuclideanPatternCommand(LaneStateAccess& access, int laneIndex, int hits, int steps, int rotation);
};

// Step editing (sec. 24.2.3); out-of-range step indices leave the lane unchanged.
class SetStepOnCommand final : public FunctionLaneCommand {
public:
    SetStepOnCommand(LaneStateAccess& access, int laneIndex, int stepIndex, bool on);
};
class SetStepVelocityCommand final : public FunctionLaneCommand {
public:
    SetStepVelocityCommand(LaneStateAccess& access, int laneIndex, int stepIndex, int value);
};
class ClearLaneCommand final : public FunctionLaneCommand {
public:
    ClearLaneCommand(LaneStateAccess& access, int laneIndex);
};
class FillLaneCommand final : public FunctionLaneCommand {
public:
    FillLaneCommand(LaneStateAccess& access, int laneIndex);
};

// Configuration (sec. 24.2.4).
class SetInputModeCommand final : public FunctionLaneCommand {
public:
    SetInputModeCommand(LaneStateAccess& access, int laneIndex, sequencing::InputMode mode);
};
class SetFollowIndexCommand final : public FunctionLaneCommand {
public:
    SetFollowIndexCommand(LaneStateAccess& access, int laneIndex, int index);
};
class SetFollowChannelCommand final : public FunctionLaneCommand {
public:
    SetFollowChannelCommand(LaneStateAccess& access, int laneIndex, int channel);
};
class SetOutputChannelCommand final : public FunctionLaneCommand {
public:
    SetOutputChannelCommand(LaneStateAccess& access, int laneIndex, int channel);
};
class SetTransposeCommand final : public FunctionLaneCommand {
public:
    SetTransposeCommand(LaneStateAccess& access, int laneIndex, int semitones);
};
class SetGateCommand final : public FunctionLaneCommand {
public:
    SetGateCommand(LaneStateAccess& access, int laneIndex, int percent);
};
class SetVelocityModeCommand final : public FunctionLaneCommand {
public:
    SetVelocityModeCommand(LaneStateAccess& access, int laneIndex, sequencing::VelocityMode mode);
};
class SetStepCountCommand final : public FunctionLaneCommand {
public:
    SetStepCountCommand(LaneStateAccess& access, int laneIndex, int count);
};
class SetTimingSourceCommand final : public FunctionLaneCommand {
public:
    SetTimingSourceCommand(LaneStateAccess& access, int laneIndex, TimingSource source);
};
class SetBaseTimingCommand final : public FunctionLaneCommand {
public:
    SetBaseTimingCommand(LaneStateAccess& access, int laneIndex, int baseTimingIndex, sequencing::Subdivision subdivision, int swingPercent);
};
class SetMuteCommand final : public FunctionLaneCommand {
public:
    SetMuteCommand(LaneStateAccess& access, int laneIndex, bool mute);
};
class SetLatchCommand final : public FunctionLaneCommand {
public:
    SetLatchCommand(LaneStateAccess& access, int laneIndex, bool latch);
};
class SetLaneColorCommand final : public FunctionLaneCommand {
public:
    SetLaneColorCommand(LaneStateAccess& access, int laneIndex, LaneColor color);
};
class SetTextGridCommand final : public FunctionLaneCommand {
public:
    SetTextGridCommand(LaneStateAccess& access, int laneIndex, std::string text);
};

}  // namespace phu::arpeggiator
