#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace phu::plugin::editor {

// UI-only colours (not spec constants, sec. 14) and the shared dark theme.
class EditorLookAndFeel final : public juce::LookAndFeel_V4 {
public:
    EditorLookAndFeel()
    {
        setColour(juce::ResizableWindow::backgroundColourId, background());
        setColour(juce::ComboBox::backgroundColourId, panel());
        setColour(juce::TextButton::buttonColourId, panel());
        setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff3a6ea5));
        setColour(juce::Slider::backgroundColourId, panel());
        setColour(juce::Slider::trackColourId, juce::Colour(0xff3a6ea5));
        setColour(juce::TextEditor::backgroundColourId, panel());
    }

    [[nodiscard]] static juce::Colour background() { return juce::Colour(0xff1b1d21); }
    [[nodiscard]] static juce::Colour panel() { return juce::Colour(0xff2a2d33); }
    [[nodiscard]] static juce::Colour neutralStep() { return juce::Colour(0xff4a4e56); }
    [[nodiscard]] static juce::Colour amber() { return juce::Colour(0xffffb000); }
    [[nodiscard]] static juce::Colour invalidMarker() { return juce::Colour(0xffe53935); }
};

}  // namespace phu::plugin::editor
