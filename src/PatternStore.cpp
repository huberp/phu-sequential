#include "PatternStore.h"

#include <utility>

namespace phu::plugin {

void PatternStore::setStepEnabled(int lane, int step, bool enabled) {
    {
        const std::lock_guard lock(mutex_);
        document_.setStepEnabled(lane, step, enabled);
    }
    sendChangeMessage();
}

void PatternStore::setStepValue(int lane, int step, sequencing::VelocityMode mode, int value) {
    {
        const std::lock_guard lock(mutex_);
        document_.setStepValue(lane, step, mode, value);
    }
    sendChangeMessage();
}

sequencing::TextGridParseResult PatternStore::setTextGridText(int lane, std::string text) {
    sequencing::TextGridParseResult result;
    {
        const std::lock_guard lock(mutex_);
        result = document_.setTextGridText(lane, std::move(text));
    }
    sendChangeMessage();
    return result;
}

void PatternStore::setLaneColor(int lane, arpeggiator::LaneColor color) {
    {
        const std::lock_guard lock(mutex_);
        document_.setLaneColor(lane, color);
    }
    sendChangeMessage();
}

void PatternStore::replaceDocument(arpeggiator::PatternDocument document) {
    {
        const std::lock_guard lock(mutex_);
        document_ = std::move(document);
    }
    sendChangeMessage();
}

arpeggiator::PatternDocument PatternStore::copyDocument() const {
    const std::lock_guard lock(mutex_);
    return document_;
}

}  // namespace phu::plugin
