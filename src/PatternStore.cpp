#include "PatternStore.h"

#include "sequencing/Constants.h"

#include <utility>

namespace phu::plugin {

PatternStore::PatternStore() {
    const std::lock_guard lock(mutex_);
    publishLocked();
}

void PatternStore::publishLocked() {
    toAudio_.writeBuffer() = document_.makeSnapshot();
    toAudio_.publish();
}

void PatternStore::setStepEnabled(int lane, int step, bool enabled) {
    {
        const std::lock_guard lock(mutex_);
        document_.setStepEnabled(lane, step, enabled);
        publishLocked();
    }
    sendChangeMessage();
}

void PatternStore::setStepValue(int lane, int step, sequencing::VelocityMode mode, int value) {
    {
        const std::lock_guard lock(mutex_);
        document_.setStepValue(lane, step, mode, value);
        publishLocked();
    }
    sendChangeMessage();
}

sequencing::TextGridParseResult PatternStore::setTextGridText(int lane, std::string text) {
    sequencing::TextGridParseResult result;
    {
        const std::lock_guard lock(mutex_);
        result = document_.setTextGridText(lane, std::move(text));
        publishLocked();
    }
    sendChangeMessage();
    return result;
}

void PatternStore::setLaneColor(int lane, arpeggiator::LaneColor color) {
    {
        const std::lock_guard lock(mutex_);
        document_.setLaneColor(lane, color);
        publishLocked();
    }
    sendChangeMessage();
}

void PatternStore::replaceDocument(arpeggiator::PatternDocument document) {
    {
        const std::lock_guard lock(mutex_);
        // Lane-wise adoption instead of assignment: assignment would also adopt the incoming
        // document's (possibly lower) revision counter.
        for (int lane = 0; lane < sequencing::constants::lanes::laneCount; ++lane) {
            document_.setLane(lane, document.lane(lane));
        }
        publishLocked();
    }
    sendChangeMessage();
}

arpeggiator::PatternDocument PatternStore::copyDocument() const {
    const std::lock_guard lock(mutex_);
    return document_;
}

}  // namespace phu::plugin