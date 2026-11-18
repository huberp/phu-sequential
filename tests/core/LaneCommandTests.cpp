#include <catch2/catch_test_macros.hpp>

#include "arpeggiator/LaneCommand.h"
#include "arpeggiator/LaneCommandRegistry.h"
#include "arpeggiator/LaneCommands.h"
#include "sequencing/StepData.h"
#include "sequencing/UndoManager.h"

#include <algorithm>
#include <vector>

#include <array>
#include <memory>
#include <string>

// M27 base class: LaneCommand over an in-memory LaneStateAccess fake (sec. 23.3, 23.6).

using namespace phu::arpeggiator;
using namespace phu::sequencing;
using namespace phu::sequencing::constants;

namespace {

class FakeAccess final : public LaneStateAccess {
public:
    FakeAccess() {
        for (int i = 0; i < lanes::laneCount; ++i) {
            lanes_[static_cast<std::size_t>(i)] = defaultLaneState(i);
        }
    }
    [[nodiscard]] LaneState get(int lane) const override { return lanes_[static_cast<std::size_t>(lane)]; }
    void set(int lane, const LaneState& state) override { lanes_[static_cast<std::size_t>(lane)] = state; }

private:
    std::array<LaneState, lanes::laneCount> lanes_;
};

class TestTransposeCommand final : public LaneCommand {
public:
    TestTransposeCommand(LaneStateAccess& access, int lane, int transpose)
        : LaneCommand(access, lane), transpose_(transpose) {}
    [[nodiscard]] std::string getName() const override { return "Set Transpose"; }

protected:
    void doExecute(LaneState& lane) override {
        lane.configuration.transpose = transpose_;
        lane.document.pattern.steps.setEnabled(0, false);
    }

private:
    int transpose_;
};

}  // namespace

TEST_CASE("LaneCommand: undo yields a state equal to the saved LaneState", "[M27][M26][REQ-23-4]") {
    FakeAccess access;
    const auto before = access.get(3);
    const auto otherBefore = access.get(2);

    UndoManager manager;
    manager.execute(std::make_unique<TestTransposeCommand>(access, 3, 5));
    CHECK(access.get(3).configuration.transpose == 5);
    CHECK_FALSE(access.get(3) == before);
    CHECK(access.get(2) == otherBefore);

    manager.undo();
    CHECK(access.get(3) == before);

    manager.redo();
    CHECK(access.get(3).configuration.transpose == 5);
    manager.undo();
    CHECK(access.get(3) == before);
}

TEST_CASE("LaneCommand: memento is captured at execute time", "[M27][REQ-23-4]") {
    FakeAccess access;
    UndoManager manager;
    manager.execute(std::make_unique<TestTransposeCommand>(access, 0, 1));
    manager.execute(std::make_unique<TestTransposeCommand>(access, 0, 2));
    manager.undo();
    CHECK(access.get(0).configuration.transpose == 1);
    manager.undo();
    CHECK(access.get(0) == defaultLaneState(0));
}

namespace {

std::string enabledBits(const LaneState& lane, int count) {
    std::string bits;
    for (int i = 0; i < count; ++i) {
        bits += lane.document.pattern.steps.step(i).enabled ? '1' : '0';
    }
    return bits;
}

void run(UndoManager& manager, std::unique_ptr<Command> command) {
    manager.execute(std::move(command));
}

}  // namespace

TEST_CASE("Lane commands: step manipulation acts on the active step count", "[M27][REQ-24-1]") {
    FakeAccess access;
    UndoManager manager;
    run(manager, std::make_unique<SetStepCountCommand>(access, 0, 4));
    run(manager, std::make_unique<ClearLaneCommand>(access, 0));
    run(manager, std::make_unique<SetStepOnCommand>(access, 0, 1, true));
    CHECK(enabledBits(access.get(0), 4) == "0100");

    run(manager, std::make_unique<SetStepOnCommand>(access, 0, 9, true));  // beyond the step count: ignored
    CHECK(access.get(0).document.pattern.steps.step(9) == defaultStepData());

    run(manager, std::make_unique<RotateStepsCommand>(access, 0, 1));
    CHECK(enabledBits(access.get(0), 4) == "0010");
    run(manager, std::make_unique<RotateStepsCommand>(access, 0, -2));
    CHECK(enabledBits(access.get(0), 4) == "1000");
    run(manager, std::make_unique<ReverseStepsCommand>(access, 0));
    CHECK(enabledBits(access.get(0), 4) == "0001");
    run(manager, std::make_unique<InvertStepsCommand>(access, 0));
    CHECK(enabledBits(access.get(0), 4) == "1110");
    run(manager, std::make_unique<FillLaneCommand>(access, 0));
    CHECK(enabledBits(access.get(0), 4) == "1111");
    CHECK(access.get(0).document.pattern.steps.step(4) == defaultStepData());

    while (manager.canUndo()) {
        manager.undo();
    }
    CHECK(access.get(0) == defaultLaneState(0));
}

TEST_CASE("Lane commands: step values use the active velocity mode and are clamped", "[M27][REQ-24-1]") {
    FakeAccess access;
    UndoManager manager;
    run(manager, std::make_unique<SetVelocityModeCommand>(access, 1, VelocityMode::byStep));
    run(manager, std::make_unique<SetStepVelocityCommand>(access, 1, 2, 500));
    const auto byStep = static_cast<std::size_t>(VelocityMode::byStep);
    CHECK(access.get(1).document.pattern.steps.step(2).valueByMode[byStep] == midi::highestVelocity);

    run(manager, std::make_unique<ArithmeticSequenceCommand>(access, 1, 10, 20, 4));
    const auto& steps = access.get(1).document.pattern.steps;
    CHECK(steps.step(0).valueByMode[byStep] == 10);
    CHECK(steps.step(1).valueByMode[byStep] == 30);
    CHECK(steps.step(3).valueByMode[byStep] == 70);
    CHECK(steps.step(4).valueByMode[byStep] == velocity::defaultByStepValue);
}

TEST_CASE("Lane commands: euclidean pattern", "[M27][REQ-24-1]") {
    FakeAccess access;
    UndoManager manager;
    run(manager, std::make_unique<EuclideanPatternCommand>(access, 0, 4, 8, 0));
    CHECK(enabledBits(access.get(0), 8) == "10101010");
    run(manager, std::make_unique<EuclideanPatternCommand>(access, 0, 3, 8, 0));
    const auto bits = enabledBits(access.get(0), 8);
    CHECK(std::count(bits.begin(), bits.end(), '1') == 3);
    run(manager, std::make_unique<EuclideanPatternCommand>(access, 0, 99, 99, 0));  // clamped to the step count
    CHECK(enabledBits(access.get(0), lanes::defaultStepCount) == std::string(lanes::defaultStepCount, '1'));
}

TEST_CASE("Lane commands: configuration commands clamp to the spec ranges", "[M27][REQ-24-1]") {
    FakeAccess access;
    UndoManager manager;
    run(manager, std::make_unique<SetGateCommand>(access, 2, 999));
    CHECK(access.get(2).configuration.gatePercent == lanes::maximumGatePercent);
    run(manager, std::make_unique<SetGateCommand>(access, 2, 0));
    CHECK(access.get(2).configuration.gatePercent == lanes::minimumGatePercent);
    run(manager, std::make_unique<SetTransposeCommand>(access, 2, -99));
    CHECK(access.get(2).configuration.transpose == lanes::minimumTranspose);
    run(manager, std::make_unique<SetFollowIndexCommand>(access, 2, 99));
    CHECK(access.get(2).configuration.followIndex == lanes::maximumFollowIndex);
    run(manager, std::make_unique<SetFollowChannelCommand>(access, 2, 0));
    CHECK(access.get(2).configuration.followChannel == midi::firstChannel);
    run(manager, std::make_unique<SetOutputChannelCommand>(access, 2, 99));
    CHECK(access.get(2).configuration.outputChannel == midi::lastChannel);
    run(manager, std::make_unique<SetBaseTimingCommand>(access, 2, 99, Subdivision::dotted, 500));
    CHECK(access.get(2).configuration.swingPercent == lanes::maximumSwingPercent);
    CHECK(access.get(2).configuration.subdivision == Subdivision::dotted);
    run(manager, std::make_unique<SetInputModeCommand>(access, 2, InputMode::channel));
    run(manager, std::make_unique<SetTimingSourceCommand>(access, 2, TimingSource::textGrid));
    run(manager, std::make_unique<SetMuteCommand>(access, 2, true));
    run(manager, std::make_unique<SetLatchCommand>(access, 2, true));
    run(manager, std::make_unique<SetLaneColorCommand>(access, 2, LaneColor{1, 2, 3}));
    const auto lane = access.get(2);
    CHECK(lane.configuration.inputMode == InputMode::channel);
    CHECK(lane.configuration.timingSource == TimingSource::textGrid);
    CHECK(lane.configuration.mute);
    CHECK(lane.configuration.latch);
    CHECK(lane.document.color == LaneColor{1, 2, 3});

    run(manager, std::make_unique<SetStepCountCommand>(access, 2, 999));
    CHECK(access.get(2).configuration.stepCount == lanes::maximumStepCount);
    while (manager.canUndo()) {
        manager.undo();
    }
    CHECK(access.get(2) == defaultLaneState(2));
}

TEST_CASE("Lane commands: shrinking the step count resets steps beyond it and undo restores them", "[M27][REQ-24-1]") {
    FakeAccess access;
    UndoManager manager;
    run(manager, std::make_unique<SetStepOnCommand>(access, 0, 10, false));
    const auto before = access.get(0);
    run(manager, std::make_unique<SetStepCountCommand>(access, 0, 4));
    CHECK(access.get(0).document.pattern.steps.step(10) == defaultStepData());
    manager.undo();
    CHECK(access.get(0) == before);
    CHECK_FALSE(access.get(0).document.pattern.steps.step(10).enabled);
}

TEST_CASE("Lane commands: text grid is always stored but applied only when valid", "[M27][REQ-24-1]") {
    FakeAccess access;
    UndoManager manager;
    run(manager, std::make_unique<SetTextGridCommand>(access, 0, "1/8, 1/8t, 1/4"));
    const auto applied = access.get(0).document.pattern.appliedTextGrid;
    CHECK(static_cast<std::string>(access.get(0).document.textGridText) == "1/8, 1/8t, 1/4");
    run(manager, std::make_unique<SetTextGridCommand>(access, 0, "1/8,,"));
    CHECK(static_cast<std::string>(access.get(0).document.textGridText) == "1/8,,");
    CHECK(access.get(0).document.pattern.appliedTextGrid == applied);
    manager.undo();
    manager.undo();
    CHECK(access.get(0) == defaultLaneState(0));
}

TEST_CASE("Lane commands: FillRandom is deterministic for equal seed and redo", "[M27][REQ-24-2]") {
    FakeAccess a;
    FakeAccess b;
    UndoManager manager;
    manager.execute(std::make_unique<FillRandomCommand>(a, 0, 50, 1234U));
    const auto first = a.get(0);
    manager.undo();
    CHECK(a.get(0) == defaultLaneState(0));
    manager.redo();
    CHECK(a.get(0) == first);

    UndoManager other;
    other.execute(std::make_unique<FillRandomCommand>(b, 0, 50, 1234U));
    CHECK(b.get(0) == first);

    UndoManager different;
    different.execute(std::make_unique<FillRandomCommand>(b, 1, 50, 99U));
    CHECK(enabledBits(b.get(1), lanes::defaultStepCount) != enabledBits(first, lanes::defaultStepCount));

    UndoManager extremes;
    extremes.execute(std::make_unique<FillRandomCommand>(b, 2, 0, 5U));
    CHECK(enabledBits(b.get(2), 8) == "00000000");
    extremes.execute(std::make_unique<FillRandomCommand>(b, 2, 100, 5U));
    CHECK(enabledBits(b.get(2), 8) == "11111111");
}

TEST_CASE("Lane commands: macro is one undo step and reverts in reverse order", "[M27][M25][REQ-24-3]") {
    FakeAccess access;
    const auto original = access.get(0);
    UndoManager manager;
    std::vector<std::unique_ptr<Command>> commands;
    commands.push_back(std::make_unique<FillRandomCommand>(access, 0, 50, 7U));
    commands.push_back(std::make_unique<RotateStepsCommand>(access, 0, 2));
    manager.executeMacro(std::move(commands));
    CHECK_FALSE(access.get(0) == original);
    const auto after = access.get(0);

    manager.undo();
    CHECK(access.get(0) == original);
    CHECK_FALSE(manager.canUndo());
    manager.redo();
    CHECK(access.get(0) == after);
}

TEST_CASE("LaneCommandRegistry: create by name, unknown name yields null, names are listed", "[M27][REQ-24-4]") {
    FakeAccess access;
    LaneCommandRegistry registry;
    CHECK(registry.getCommandNames().empty());
    CHECK(registry.createCommand("Nope", access, 0) == nullptr);

    registry.registerCommand("Clear", [](LaneStateAccess& s, int l, const CommandArguments&) {
        return std::make_unique<ClearLaneCommand>(s, l);
    });
    CHECK(registry.getCommandNames() == std::vector<std::string>{"Clear"});
    auto command = registry.createCommand("Clear", access, 0);
    REQUIRE(command != nullptr);
    command->execute();
    CHECK(enabledBits(access.get(0), 8) == "00000000");

    LaneCommandRegistry defaults;
    defaults.registerDefaultCommands();
    CHECK(defaults.getCommandNames().size() == 24);
    CommandArguments args;
    args.numbers = {100, 5};
    auto fill = defaults.createCommand("FillRandom", access, 0, args);
    REQUIRE(fill != nullptr);
    fill->execute();
    CHECK(enabledBits(access.get(0), 8) == "11111111");
    CommandArguments text;
    text.text = "1/4";
    auto grid = defaults.createCommand("SetTextGrid", access, 0, text);
    REQUIRE(grid != nullptr);
    CHECK(static_cast<std::string>(grid->getName()) == "Set Text Grid");
}
