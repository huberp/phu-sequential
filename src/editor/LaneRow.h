#pragma once

#include "PatternStore.h"
#include "arpeggiator/DisplayScale.h"
#include "arpeggiator/LaneConfiguration.h"
#include "arpeggiator/PatternDocument.h"
#include "editor/LaneControlStrip.h"
#include "editor/StepDisplay.h"
#include "editor/TextGridField.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace phu::plugin::editor {

// One lane: sidebar + control strip | text grid field above the step display; lane number badge in lane color. [MT]
class LaneRow final : public juce::Component {
public:
    LaneRow(juce::AudioProcessorValueTreeState&, PatternStore&, int laneIndex);

    void update(const arpeggiator::LaneParameters&, const arpeggiator::LaneDocument&,
                const arpeggiator::LaneDisplayGeometry&);
    void setScrollOffset(int pixels) { stepDisplay_.setScrollOffset(pixels); }
    void setCurrentStep(int stepIndex) { stepDisplay_.setCurrentStep(stepIndex); }

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    int laneIndex_;
    juce::Colour laneColour_;
    LaneControlStrip strip_;
    TextGridField textGrid_;
    StepDisplay stepDisplay_;
};

}  // namespace phu::plugin::editor
