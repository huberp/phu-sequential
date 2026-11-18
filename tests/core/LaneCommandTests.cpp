#include <catch2/catch_test_macros.hpp>

#include "arpeggiator/LaneCommand.h"
#include "sequencing/UndoManager.h"

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

class SetTransposeCommand final : public LaneCommand {
public:
    SetTransposeCommand(LaneStateAccess& access, int lane, int transpose)
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
    manager.execute(std::make_unique<SetTransposeCommand>(access, 3, 5));
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
    manager.execute(std::make_unique<SetTransposeCommand>(access, 0, 1));
    manager.execute(std::make_unique<SetTransposeCommand>(access, 0, 2));
    manager.undo();
    CHECK(access.get(0).configuration.transpose == 1);
    manager.undo();
    CHECK(access.get(0) == defaultLaneState(0));
}
