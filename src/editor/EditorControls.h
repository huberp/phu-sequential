#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace phu::plugin::editor {

// Fills a combo box with the choices of an AudioParameterChoice (item id = index + 1) before it is attached.
inline void populateChoices(juce::ComboBox& box, juce::RangedAudioParameter& parameter)
{
    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*>(&parameter)) {
        box.addItemList(choice->choices, 1);
    }
}

// Compact bar slider showing "<label> <value>"; no text entry.
inline void configureBarSlider(juce::Slider& slider, const juce::String& label)
{
    slider.setSliderStyle(juce::Slider::LinearBar);
    slider.setTextBoxIsEditable(false);
    slider.setTextBoxStyle(juce::Slider::TextBoxLeft, true, 0, 0);
    slider.setName(label);
}

// Must be called after the SliderParameterAttachment is created (it installs its own text functions).
inline void setBarSliderText(juce::Slider& slider, const juce::String& label, const juce::String& suffix = {})
{
    slider.textFromValueFunction = [label, suffix](double value) {
        return label + " " + juce::String(juce::roundToInt(value)) + suffix;
    };
    slider.updateText();
}

}  // namespace phu::plugin::editor
