#pragma once

#include "arpeggiator/PatternDocument.h"

#include <juce_events/juce_events.h>

#include <mutex>

namespace phu::plugin {

// Owner of all non-parameter state on the non-audio side (sec. 12). Every access is serialized by an
// internal mutex that the audio thread never touches. [MT]
class PatternStore : public juce::ChangeBroadcaster {
public:
    void setStepEnabled(int lane, int step, bool enabled);
    void setStepValue(int lane, int step, sequencing::VelocityMode, int value);
    sequencing::TextGridParseResult setTextGridText(int lane, std::string text);
    void setLaneColor(int lane, arpeggiator::LaneColor);

    // State/preset load: replaces everything at once.
    void replaceDocument(arpeggiator::PatternDocument);
    [[nodiscard]] arpeggiator::PatternDocument copyDocument() const;

private:
    mutable std::mutex mutex_;
    arpeggiator::PatternDocument document_;
};

}  // namespace phu::plugin
