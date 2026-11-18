#include "arpeggiator/LaneState.h"

namespace phu::arpeggiator {

LaneState defaultLaneState(int laneIndex) {
    return LaneState{LaneParameters{}, PatternDocument{}.lane(laneIndex)};
}

}  // namespace phu::arpeggiator
