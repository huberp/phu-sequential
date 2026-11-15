#include "PluginProcessor.h"
#include "HostTransport.h"
#include "PluginEditor.h"

PluginProcessor::PluginProcessor() : juce::AudioProcessor(
    juce::AudioProcessor::BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
}

PluginProcessor::~PluginProcessor()
{
}

const juce::String PluginProcessor::getName() const
{
    return JucePlugin_Name;
}

bool PluginProcessor::acceptsMidi() const
{
    return true;
}

bool PluginProcessor::producesMidi() const
{
    return true;
}

bool PluginProcessor::isMidiEffect() const
{
    return true;
}

double PluginProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int PluginProcessor::getNumPrograms()
{
    return 1;
}

int PluginProcessor::getCurrentProgram()
{
    return 0;
}

void PluginProcessor::setCurrentProgram(int)
{
}

const juce::String PluginProcessor::getProgramName(int)
{
    return {};
}

void PluginProcessor::changeProgramName(int, const juce::String&)
{
}

void PluginProcessor::prepareToPlay(double sampleRate, int)
{
    sampleRate_ = sampleRate;
    engine_.prepare();
}

void PluginProcessor::releaseResources()
{
}

void PluginProcessor::processInternal(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi, bool bypassed)
{
    const auto transport = phu::plugin::readHostTransport(getPlayHead());
    engine_.process(midi, buffer.getNumSamples(), transport, sampleRate_, bypassed, config_);
}

void PluginProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    processInternal(buffer, midi, false);
}

void PluginProcessor::processBlockBypassed(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    processInternal(buffer, midi, true);
}

juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
    return new PluginEditor(*this);
}

bool PluginProcessor::hasEditor() const
{
    return true;
}

void PluginProcessor::getStateInformation(juce::MemoryBlock&)
{
}

void PluginProcessor::setStateInformation(const void*, int)
{
}

// JUCE plugin wrapper macro
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}
