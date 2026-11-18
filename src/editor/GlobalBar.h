#pragma once

#include "LaneEditHistory.h"
#include "PresetManager.h"
#include "editor/EditorControls.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <memory>

namespace phu::plugin::editor {

// Thru, Consume, Chord Input Channel, undo/redo and the preset browser (sec. 13, 14). [MT]
class GlobalBar final : public juce::Component {
public:
    GlobalBar(juce::AudioProcessorValueTreeState&, PresetManager&, LaneEditHistory&);

    void refreshPresetName();
    void refreshUndoState();  // enables the buttons from canUndo()/canRedo()
    void resized() override;

private:
    void showLoadMenu();
    void showSaveDialog();

    PresetManager& presets_;
    LaneEditHistory& history_;
    juce::ToggleButton thru_{"Thru"};
    juce::ToggleButton consume_{"Consume"};
    juce::Slider chordInputChannel_;
    juce::TextButton undo_{"Undo"};
    juce::TextButton redo_{"Redo"};
    juce::Label presetName_;
    juce::TextButton previous_{juce::String::fromUTF8("\xe2\x97\x82")};
    juce::TextButton next_{juce::String::fromUTF8("\xe2\x96\xb8")};
    juce::TextButton save_{juce::String::fromUTF8("Save\xe2\x80\xa6")};
    juce::TextButton load_{juce::String::fromUTF8("Load\xe2\x80\xa6")};

    std::unique_ptr<juce::ButtonParameterAttachment> thruAttachment_;
    std::unique_ptr<juce::ButtonParameterAttachment> consumeAttachment_;
    std::unique_ptr<juce::SliderParameterAttachment> channelAttachment_;
};

}  // namespace phu::plugin::editor
