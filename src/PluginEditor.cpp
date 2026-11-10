#include "PluginEditor.h"
#include "PluginProcessor.h"

PluginEditor::PluginEditor(PluginProcessor& processor)
    : juce::AudioProcessorEditor(&processor), processorRef(processor)
{
    setSize(400, 300);
}

void PluginEditor::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(16.0f));
    g.drawFittedText("phu-sequential", getLocalBounds(), juce::Justification::centred, 1);
}

void PluginEditor::resized()
{
}
