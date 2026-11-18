#include "LaneEditHistory.h"

#include "StateSerializer.h"

namespace phu::plugin {

LaneEditHistory::LaneEditHistory(juce::AudioProcessorValueTreeState& apvts, PatternStore& store)
    : apvts_(apvts), store_(store) {
    registry_.registerDefaultCommands();
}

arpeggiator::LaneState LaneEditHistory::get(int laneIndex) const {
    return captureLaneState(apvts_, store_, laneIndex);
}

void LaneEditHistory::set(int laneIndex, const arpeggiator::LaneState& state) {
    applyLaneState(state, laneIndex, apvts_, store_);
}

void LaneEditHistory::execute(
    const std::function<std::unique_ptr<arpeggiator::LaneCommand>(arpeggiator::LaneStateAccess&)>& makeCommand) {
    coalesceKey_.clear();
    undoManager_.execute(makeCommand(*this));
}

bool LaneEditHistory::execute(const std::string& name, int laneIndex, const arpeggiator::CommandArguments& args) {
    auto command = registry_.createCommand(name, *this, laneIndex, args);
    if (!command) {
        return false;
    }
    coalesceKey_.clear();
    undoManager_.execute(std::move(command));
    return true;
}

void LaneEditHistory::executeCoalesced(
    const std::string& key,
    const std::function<std::unique_ptr<arpeggiator::LaneCommand>(arpeggiator::LaneStateAccess&)>& makeCommand) {
    if (coalesceKey_ == key && undoManager_.canUndo()) {
        undoManager_.undo();  // the new command captures the state from before the gesture
    }
    undoManager_.execute(makeCommand(*this));
    coalesceKey_ = key;
}

void LaneEditHistory::undo() {
    coalesceKey_.clear();
    undoManager_.undo();
}

void LaneEditHistory::redo() {
    coalesceKey_.clear();
    undoManager_.redo();
}

void LaneEditHistory::clear() {
    coalesceKey_.clear();
    undoManager_.clear();
}

}  // namespace phu::plugin
