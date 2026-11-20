#include "arpeggiator/LaneCommand.h"

#include "sequencing/Constants.h"

#include <algorithm>

namespace phu::arpeggiator {

LaneCommand::LaneCommand(LaneStateAccess& access, int laneIndex)
    : access_(access),
      laneIndex_(std::clamp(laneIndex, 0, sequencing::constants::lanes::laneCount - 1)),
      savedState_(defaultLaneState(laneIndex_)) {}

void LaneCommand::execute() {
    savedState_ = access_.get(laneIndex_);
    auto modified = savedState_;
    doExecute(modified);
    access_.set(laneIndex_, modified);
}

void LaneCommand::undo() {
    access_.set(laneIndex_, savedState_);
}

}  // namespace phu::arpeggiator
