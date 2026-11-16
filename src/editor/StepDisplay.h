#pragma once

#include "PatternStore.h"
#include "arpeggiator/DisplayScale.h"
#include "sequencing/StepData.h"
#include "sequencing/velocity/VelocityMode.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace phu::plugin::editor {

struct StepDisplayState {
    arpeggiator::LaneDisplayGeometry geometry;
    sequencing::StepPattern pattern;
    sequencing::VelocityMode velocityMode = sequencing::VelocityMode::incoming;
    juce::Colour laneColour;
    bool muted = false;

    friend bool operator==(const StepDisplayState&, const StepDisplayState&) = default;
};

// Steps at DisplayScale geometry: pad (click = on/off) above a velocity drag bar per step (sec. 14). [MT]
class StepDisplay final : public juce::Component {
public:
    StepDisplay(PatternStore&, int laneIndex);

    void setState(const StepDisplayState&);
    void setScrollOffset(int pixels);
    void setCurrentStep(int stepIndex);  // -1: no indicator

    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;

private:
    [[nodiscard]] int stepAt(int x) const;
    [[nodiscard]] juce::Rectangle<int> stepBounds(int step) const;
    [[nodiscard]] juce::Rectangle<int> padBounds(int step) const;
    [[nodiscard]] juce::Rectangle<int> barBounds(int step) const;
    [[nodiscard]] bool barsEnabled() const;
    [[nodiscard]] int valueAt(int step, int y) const;
    void applyDrag(int y);

    PatternStore& store_;
    int laneIndex_;
    StepDisplayState state_;
    int scrollOffset_ = 0;
    int currentStep_ = -1;
    int draggedStep_ = -1;
};

}  // namespace phu::plugin::editor
