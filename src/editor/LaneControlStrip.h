#pragma once

#include "PatternStore.h"
#include "arpeggiator/LaneConfiguration.h"
#include "editor/EditorControls.h"
#include "editor/EditorLayout.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <initializer_list>
#include <memory>

namespace phu::plugin::editor {

// Swatch button; click opens a CallOutBox with a ColourSelector (no alpha). Affects only this lane (sec. 14). [MT]
class LaneColorPicker final : public juce::Component {
public:
    LaneColorPicker(PatternStore&, int laneIndex);

    void setColour(juce::Colour);
    void paint(juce::Graphics&) override;
    void mouseUp(const juce::MouseEvent&) override;

private:
    PatternStore& store_;
    int laneIndex_;
    juce::Colour colour_;
};

// All lane parameters via APVTS attachments (sec. 12, 14). Lays itself out into the regions of a lane row. [MT]
class LaneControlStrip final {
public:
    LaneControlStrip(juce::AudioProcessorValueTreeState&, PatternStore&, int laneIndex, juce::Component& parent);

    void layOut(const layout::LaneRowRegions&);
    // Enabled/visible states derived from the lane parameters (sec. 14, D-46).
    void update(const arpeggiator::LaneParameters&, juce::Colour laneColour);

private:
    juce::AudioProcessorValueTreeState& apvts_;
    int laneIndex_;

    juce::TextButton mute_{"M"};
    juce::TextButton latch_{"L"};
    juce::ComboBox inputMode_;
    juce::Slider followIndex_;
    juce::Slider followChannel_;
    juce::Slider outputChannel_;
    juce::Slider transpose_;
    juce::Slider gate_;
    juce::TextButton timingBase_{"Base"};
    juce::TextButton timingGrid_{"Grid"};
    juce::ComboBox baseTiming_;
    juce::ComboBox subdivision_;
    juce::Slider swing_;
    juce::Slider stepCount_;
    juce::ComboBox velocityMode_;
    LaneColorPicker colorPicker_;

    std::unique_ptr<juce::ButtonParameterAttachment> muteAttachment_;
    std::unique_ptr<juce::ButtonParameterAttachment> latchAttachment_;
    std::unique_ptr<juce::ComboBoxParameterAttachment> inputModeAttachment_;
    std::unique_ptr<juce::SliderParameterAttachment> followIndexAttachment_;
    std::unique_ptr<juce::SliderParameterAttachment> followChannelAttachment_;
    std::unique_ptr<juce::SliderParameterAttachment> outputChannelAttachment_;
    std::unique_ptr<juce::SliderParameterAttachment> transposeAttachment_;
    std::unique_ptr<juce::SliderParameterAttachment> gateAttachment_;
    std::unique_ptr<juce::ParameterAttachment> timingSourceAttachment_;
    std::unique_ptr<juce::ComboBoxParameterAttachment> baseTimingAttachment_;
    std::unique_ptr<juce::ComboBoxParameterAttachment> subdivisionAttachment_;
    std::unique_ptr<juce::SliderParameterAttachment> swingAttachment_;
    std::unique_ptr<juce::SliderParameterAttachment> stepCountAttachment_;
    std::unique_ptr<juce::ComboBoxParameterAttachment> velocityModeAttachment_;
};

}  // namespace phu::plugin::editor
