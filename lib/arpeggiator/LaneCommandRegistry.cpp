#include "arpeggiator/LaneCommandRegistry.h"

#include "arpeggiator/LaneCommands.h"

#include <algorithm>
#include <cstdint>
#include <utility>

namespace phu::arpeggiator {

void LaneCommandRegistry::registerCommand(const std::string& name, Factory factory) {
    const auto it = std::find_if(entries_.begin(), entries_.end(), [&](const auto& e) { return e.first == name; });
    if (it != entries_.end()) {
        it->second = std::move(factory);
    } else {
        entries_.emplace_back(name, std::move(factory));
    }
}

std::unique_ptr<LaneCommand> LaneCommandRegistry::createCommand(const std::string& name, LaneStateAccess& access,
                                                                int laneIndex, const CommandArguments& args) const {
    const auto it = std::find_if(entries_.begin(), entries_.end(), [&](const auto& e) { return e.first == name; });
    return it == entries_.end() ? nullptr : it->second(access, laneIndex, args);
}

std::vector<std::string> LaneCommandRegistry::getCommandNames() const {
    std::vector<std::string> names;
    for (const auto& entry : entries_) {
        names.push_back(entry.first);
    }
    return names;
}

void LaneCommandRegistry::registerDefaultCommands() {
    using A = const CommandArguments&;
    using sequencing::InputMode;
    using sequencing::Subdivision;
    using sequencing::VelocityMode;

    registerCommand("ReverseSteps", [](LaneStateAccess& s, int l, A) { return std::make_unique<ReverseStepsCommand>(s, l); });
    registerCommand("RotateSteps", [](LaneStateAccess& s, int l, A a) { return std::make_unique<RotateStepsCommand>(s, l, a.number(0)); });
    registerCommand("InvertSteps", [](LaneStateAccess& s, int l, A) { return std::make_unique<InvertStepsCommand>(s, l); });
    registerCommand("FillRandom", [](LaneStateAccess& s, int l, A a) {
        return std::make_unique<FillRandomCommand>(s, l, a.number(0), static_cast<std::uint32_t>(a.number(1)));
    });
    registerCommand("ArithmeticSequence", [](LaneStateAccess& s, int l, A a) {
        return std::make_unique<ArithmeticSequenceCommand>(s, l, a.number(0), a.number(1), a.number(2));
    });
    registerCommand("EuclideanPattern", [](LaneStateAccess& s, int l, A a) {
        return std::make_unique<EuclideanPatternCommand>(s, l, a.number(0), a.number(1), a.number(2));
    });
    registerCommand("SetStepOn", [](LaneStateAccess& s, int l, A a) { return std::make_unique<SetStepOnCommand>(s, l, a.number(0), a.number(1) != 0); });
    registerCommand("SetStepVelocity", [](LaneStateAccess& s, int l, A a) { return std::make_unique<SetStepVelocityCommand>(s, l, a.number(0), a.number(1)); });
    registerCommand("ClearLane", [](LaneStateAccess& s, int l, A) { return std::make_unique<ClearLaneCommand>(s, l); });
    registerCommand("FillLane", [](LaneStateAccess& s, int l, A) { return std::make_unique<FillLaneCommand>(s, l); });
    registerCommand("SetInputMode", [](LaneStateAccess& s, int l, A a) {
        return std::make_unique<SetInputModeCommand>(s, l, a.number(0) == 0 ? InputMode::chordFollow : InputMode::channel);
    });
    registerCommand("SetFollowIndex", [](LaneStateAccess& s, int l, A a) { return std::make_unique<SetFollowIndexCommand>(s, l, a.number(0)); });
    registerCommand("SetFollowChannel", [](LaneStateAccess& s, int l, A a) { return std::make_unique<SetFollowChannelCommand>(s, l, a.number(0)); });
    registerCommand("SetOutputChannel", [](LaneStateAccess& s, int l, A a) { return std::make_unique<SetOutputChannelCommand>(s, l, a.number(0)); });
    registerCommand("SetTranspose", [](LaneStateAccess& s, int l, A a) { return std::make_unique<SetTransposeCommand>(s, l, a.number(0)); });
    registerCommand("SetGate", [](LaneStateAccess& s, int l, A a) { return std::make_unique<SetGateCommand>(s, l, a.number(0)); });
    registerCommand("SetVelocityMode", [](LaneStateAccess& s, int l, A a) {
        const int mode = std::clamp(a.number(0), 0, sequencing::velocityModeCount - 1);
        return std::make_unique<SetVelocityModeCommand>(s, l, static_cast<VelocityMode>(mode));
    });
    registerCommand("SetStepCount", [](LaneStateAccess& s, int l, A a) { return std::make_unique<SetStepCountCommand>(s, l, a.number(0)); });
    registerCommand("SetTimingSource", [](LaneStateAccess& s, int l, A a) {
        return std::make_unique<SetTimingSourceCommand>(s, l, a.number(0) == 0 ? TimingSource::baseTiming : TimingSource::textGrid);
    });
    registerCommand("SetBaseTiming", [](LaneStateAccess& s, int l, A a) {
        const int subdivision = std::clamp(a.number(1), 0, sequencing::subdivisionCount - 1);
        return std::make_unique<SetBaseTimingCommand>(s, l, a.number(0), static_cast<Subdivision>(subdivision), a.number(2));
    });
    registerCommand("SetMute", [](LaneStateAccess& s, int l, A a) { return std::make_unique<SetMuteCommand>(s, l, a.number(0) != 0); });
    registerCommand("SetLatch", [](LaneStateAccess& s, int l, A a) { return std::make_unique<SetLatchCommand>(s, l, a.number(0) != 0); });
    registerCommand("SetLaneColor", [](LaneStateAccess& s, int l, A a) {
        const auto channel = [&](std::size_t i) { return static_cast<std::uint8_t>(std::clamp(a.number(i), 0, 255)); };
        return std::make_unique<SetLaneColorCommand>(s, l, LaneColor{channel(0), channel(1), channel(2)});
    });
    registerCommand("SetTextGrid", [](LaneStateAccess& s, int l, A a) { return std::make_unique<SetTextGridCommand>(s, l, a.text); });
}

}  // namespace phu::arpeggiator
