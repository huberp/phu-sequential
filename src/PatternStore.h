#pragma once

#include "arpeggiator/PatternDocument.h"
#include "arpeggiator/SequencerConfiguration.h"
#include "sequencing/TripleBuffer.h"

#include <juce_events/juce_events.h>

#include <mutex>

namespace phu::plugin {

// Owner of all non-parameter state on the non-audio side (sec. 12) and the single producer of
// PatternSnapshot for the audio thread (D-32). Every mutation is serialized by an internal mutex
// that the audio thread never touches and publishes a new snapshot. [MT]
class PatternStore : public juce::ChangeBroadcaster {
public:
    PatternStore();

    void setStepEnabled(int lane, int step, bool enabled);
    void setStepValue(int lane, int step, sequencing::VelocityMode, int value);
    sequencing::TextGridParseResult setTextGridText(int lane, std::string text);
    void setLaneColor(int lane, arpeggiator::LaneColor);

    // State/preset load: replaces all lane content at once. The published revision stays monotonic
    // so the core never ignores the loaded snapshot as stale.
    void replaceDocument(arpeggiator::PatternDocument);
    [[nodiscard]] arpeggiator::PatternDocument copyDocument() const;

    // Audio-thread consumer side ONLY (update()/read()). [RT]
    [[nodiscard]] sequencing::TripleBuffer<arpeggiator::PatternSnapshot>& audioSide() noexcept { return toAudio_; }

private:
    void publishLocked();

    mutable std::mutex mutex_;
    arpeggiator::PatternDocument document_;
    sequencing::TripleBuffer<arpeggiator::PatternSnapshot> toAudio_;
};

}  // namespace phu::plugin
