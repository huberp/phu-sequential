#include "arpeggiator/LaneCommands.h"

#include "sequencing/Constants.h"
#include "sequencing/StepData.h"
#include "sequencing/TextGridParser.h"
#include "sequencing/velocity/VelocityModeRegistry.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <utility>
#include <vector>

namespace phu::arpeggiator {

namespace {

using namespace sequencing::constants;

int activeStepCount(const LaneState& lane) noexcept {
    return effectiveStepCount(lane.configuration, lane.document.pattern.appliedTextGrid);
}

void writeStep(sequencing::StepPattern& steps, int index, const sequencing::StepData& data) noexcept {
    steps.setEnabled(index, data.enabled);
    for (int m = 0; m < sequencing::velocityModeCount; ++m) {
        steps.setValue(index, static_cast<sequencing::VelocityMode>(m), data.valueByMode[static_cast<std::size_t>(m)]);
    }
}

std::vector<sequencing::StepData> readSteps(const LaneState& lane, int count) {
    std::vector<sequencing::StepData> result;
    for (int i = 0; i < count; ++i) {
        result.push_back(lane.document.pattern.steps.step(i));
    }
    return result;
}

// Clamps the parameters and restores invariant I-1 after the parameters or the applied grid changed.
void finalize(LaneState& lane) noexcept {
    lane.configuration = clampLaneParameters(lane.configuration);
    lane.document.pattern.steps.applyStepCountChange(0, activeStepCount(lane));
}

void rotateEnabled(std::vector<bool>& values, int amount) {
    const int count = static_cast<int>(values.size());
    if (count == 0) {
        return;
    }
    const int shift = ((amount % count) + count) % count;
    std::vector<bool> rotated(values.size());
    for (int i = 0; i < count; ++i) {
        rotated[static_cast<std::size_t>((i + shift) % count)] = values[static_cast<std::size_t>(i)];
    }
    values = std::move(rotated);
}

// splitmix64: fixed algorithm so a seed yields the same pattern on every platform.
struct SeededRandom {
    std::uint64_t state;
    std::uint32_t next() noexcept {
        state += 0x9E3779B97F4A7C15ULL;
        auto z = state;
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
        return static_cast<std::uint32_t>((z ^ (z >> 31)) >> 32);
    }
};

}  // namespace

FunctionLaneCommand::FunctionLaneCommand(LaneStateAccess& access, int laneIndex, std::string name,
                                         std::function<void(LaneState&)> change)
    : LaneCommand(access, laneIndex), name_(std::move(name)), change_(std::move(change)) {}

void FunctionLaneCommand::doExecute(LaneState& lane) {
    change_(lane);
}

ReverseStepsCommand::ReverseStepsCommand(LaneStateAccess& access, int laneIndex)
    : FunctionLaneCommand(access, laneIndex, "Reverse Steps", [](LaneState& lane) {
          const int count = activeStepCount(lane);
          const auto steps = readSteps(lane, count);
          for (int i = 0; i < count; ++i) {
              writeStep(lane.document.pattern.steps, i, steps[static_cast<std::size_t>(count - 1 - i)]);
          }
      }) {}

RotateStepsCommand::RotateStepsCommand(LaneStateAccess& access, int laneIndex, int amount)
    : FunctionLaneCommand(access, laneIndex, "Rotate Steps", [amount](LaneState& lane) {
          const int count = activeStepCount(lane);
          const auto steps = readSteps(lane, count);
          const int shift = ((amount % count) + count) % count;
          for (int i = 0; i < count; ++i) {
              writeStep(lane.document.pattern.steps, (i + shift) % count, steps[static_cast<std::size_t>(i)]);
          }
      }) {}

InvertStepsCommand::InvertStepsCommand(LaneStateAccess& access, int laneIndex)
    : FunctionLaneCommand(access, laneIndex, "Invert Steps", [](LaneState& lane) {
          for (int i = 0; i < activeStepCount(lane); ++i) {
              lane.document.pattern.steps.setEnabled(i, !lane.document.pattern.steps.step(i).enabled);
          }
      }) {}

FillRandomCommand::FillRandomCommand(LaneStateAccess& access, int laneIndex, int probabilityPercent,
                                     std::uint32_t seed)
    : FunctionLaneCommand(access, laneIndex, "Fill Random", [probabilityPercent, seed](LaneState& lane) {
          const int probability = std::clamp(probabilityPercent, 0, 100);
          SeededRandom random{seed};
          for (int i = 0; i < activeStepCount(lane); ++i) {
              lane.document.pattern.steps.setEnabled(i, static_cast<int>(random.next() % 100U) < probability);
          }
      }) {}

ArithmeticSequenceCommand::ArithmeticSequenceCommand(LaneStateAccess& access, int laneIndex, int start,
                                                     int difference, int count)
    : FunctionLaneCommand(access, laneIndex, "Arithmetic Sequence", [start, difference, count](LaneState& lane) {
          const auto mode = lane.configuration.velocityMode;
          if (!sequencing::velocityModeRegistry[static_cast<std::size_t>(mode)].usesStepValues) {
              return;
          }
          const int limit = std::min(std::clamp(count, 0, lanes::maximumStepCount), activeStepCount(lane));
          for (int i = 0; i < limit; ++i) {
              // 64-bit so extreme inputs cannot overflow before clamping.
              const auto value = std::clamp<std::int64_t>(
                  static_cast<std::int64_t>(start) + static_cast<std::int64_t>(i) * difference, -1'000'000, 1'000'000);
              lane.document.pattern.steps.setValue(i, mode, static_cast<int>(value));
          }
      }) {}

EuclideanPatternCommand::EuclideanPatternCommand(LaneStateAccess& access, int laneIndex, int hits, int steps,
                                                 int rotation)
    : FunctionLaneCommand(access, laneIndex, "Euclidean Pattern", [hits, steps, rotation](LaneState& lane) {
          const int active = activeStepCount(lane);
          const int length = std::clamp(steps, 1, active);
          const int onCount = std::clamp(hits, 0, length);
          std::vector<bool> pattern(static_cast<std::size_t>(length));
          for (int i = 0; i < length; ++i) {
              pattern[static_cast<std::size_t>(i)] = (i * onCount) % length < onCount;
          }
          rotateEnabled(pattern, rotation);
          for (int i = 0; i < active; ++i) {
              lane.document.pattern.steps.setEnabled(i, i < length && pattern[static_cast<std::size_t>(i)]);
          }
      }) {}

SetStepOnCommand::SetStepOnCommand(LaneStateAccess& access, int laneIndex, int stepIndex, bool on)
    : FunctionLaneCommand(access, laneIndex, "Set Step On", [stepIndex, on](LaneState& lane) {
          if (stepIndex >= 0 && stepIndex < activeStepCount(lane)) {
              lane.document.pattern.steps.setEnabled(stepIndex, on);
          }
      }) {}

SetStepVelocityCommand::SetStepVelocityCommand(LaneStateAccess& access, int laneIndex, int stepIndex, int value)
    : FunctionLaneCommand(access, laneIndex, "Set Step Velocity", [stepIndex, value](LaneState& lane) {
          if (stepIndex >= 0 && stepIndex < activeStepCount(lane)) {
              lane.document.pattern.steps.setValue(stepIndex, lane.configuration.velocityMode, value);
          }
      }) {}

ClearLaneCommand::ClearLaneCommand(LaneStateAccess& access, int laneIndex)
    : FunctionLaneCommand(access, laneIndex, "Clear Lane", [](LaneState& lane) {
          for (int i = 0; i < activeStepCount(lane); ++i) {
              lane.document.pattern.steps.setEnabled(i, false);
          }
      }) {}

FillLaneCommand::FillLaneCommand(LaneStateAccess& access, int laneIndex)
    : FunctionLaneCommand(access, laneIndex, "Fill Lane", [](LaneState& lane) {
          for (int i = 0; i < activeStepCount(lane); ++i) {
              lane.document.pattern.steps.setEnabled(i, true);
          }
      }) {}

SetInputModeCommand::SetInputModeCommand(LaneStateAccess& access, int laneIndex, sequencing::InputMode mode)
    : FunctionLaneCommand(access, laneIndex, "Set Input Mode", [mode](LaneState& lane) {
          lane.configuration.inputMode = mode;
          finalize(lane);
      }) {}

SetFollowIndexCommand::SetFollowIndexCommand(LaneStateAccess& access, int laneIndex, int index)
    : FunctionLaneCommand(access, laneIndex, "Set Follow Index", [index](LaneState& lane) {
          lane.configuration.followIndex = index;
          finalize(lane);
      }) {}

SetFollowChannelCommand::SetFollowChannelCommand(LaneStateAccess& access, int laneIndex, int channel)
    : FunctionLaneCommand(access, laneIndex, "Set Follow Channel", [channel](LaneState& lane) {
          lane.configuration.followChannel = channel;
          finalize(lane);
      }) {}

SetOutputChannelCommand::SetOutputChannelCommand(LaneStateAccess& access, int laneIndex, int channel)
    : FunctionLaneCommand(access, laneIndex, "Set Output Channel", [channel](LaneState& lane) {
          lane.configuration.outputChannel = channel;
          finalize(lane);
      }) {}

SetTransposeCommand::SetTransposeCommand(LaneStateAccess& access, int laneIndex, int semitones)
    : FunctionLaneCommand(access, laneIndex, "Set Transpose", [semitones](LaneState& lane) {
          lane.configuration.transpose = semitones;
          finalize(lane);
      }) {}

SetGateCommand::SetGateCommand(LaneStateAccess& access, int laneIndex, int percent)
    : FunctionLaneCommand(access, laneIndex, "Set Gate", [percent](LaneState& lane) {
          lane.configuration.gatePercent = percent;
          finalize(lane);
      }) {}

SetVelocityModeCommand::SetVelocityModeCommand(LaneStateAccess& access, int laneIndex, sequencing::VelocityMode mode)
    : FunctionLaneCommand(access, laneIndex, "Set Velocity Mode", [mode](LaneState& lane) {
          lane.configuration.velocityMode = mode;
          finalize(lane);
      }) {}

SetStepCountCommand::SetStepCountCommand(LaneStateAccess& access, int laneIndex, int count)
    : FunctionLaneCommand(access, laneIndex, "Set Step Count", [count](LaneState& lane) {
          lane.configuration.stepCount = count;
          finalize(lane);
      }) {}

SetTimingSourceCommand::SetTimingSourceCommand(LaneStateAccess& access, int laneIndex, TimingSource source)
    : FunctionLaneCommand(access, laneIndex, "Set Timing Source", [source](LaneState& lane) {
          lane.configuration.timingSource = source;
          finalize(lane);
      }) {}

SetBaseTimingCommand::SetBaseTimingCommand(LaneStateAccess& access, int laneIndex, int baseTimingIndex,
                                           sequencing::Subdivision subdivision, int swingPercent)
    : FunctionLaneCommand(access, laneIndex, "Set Base Timing",
                          [baseTimingIndex, subdivision, swingPercent](LaneState& lane) {
                              lane.configuration.baseTimingIndex = baseTimingIndex;
                              lane.configuration.subdivision = subdivision;
                              lane.configuration.swingPercent = swingPercent;
                              finalize(lane);
                          }) {}

SetMuteCommand::SetMuteCommand(LaneStateAccess& access, int laneIndex, bool mute)
    : FunctionLaneCommand(access, laneIndex, "Set Mute", [mute](LaneState& lane) { lane.configuration.mute = mute; }) {}

SetLatchCommand::SetLatchCommand(LaneStateAccess& access, int laneIndex, bool latch)
    : FunctionLaneCommand(access, laneIndex, "Set Latch", [latch](LaneState& lane) { lane.configuration.latch = latch; }) {}

SetLaneColorCommand::SetLaneColorCommand(LaneStateAccess& access, int laneIndex, LaneColor color)
    : FunctionLaneCommand(access, laneIndex, "Set Lane Color", [color](LaneState& lane) { lane.document.color = color; }) {}

SetTextGridCommand::SetTextGridCommand(LaneStateAccess& access, int laneIndex, std::string text)
    : FunctionLaneCommand(access, laneIndex, "Set Text Grid", [text = std::move(text)](LaneState& lane) {
          lane.document.textGridText = text;
          if (auto parsed = sequencing::parseTextGrid(text); parsed.grid) {
              lane.document.pattern.appliedTextGrid = *parsed.grid;
          }
          finalize(lane);
      }) {}

}  // namespace phu::arpeggiator
