#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

class PluginProcessor;

class PluginEditor : public juce::AudioProcessorEditor
{
public:
    explicit PluginEditor(PluginProcessor& processor);
    ~PluginEditor() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    PluginProcessor& processorRef;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginEditor)
};
