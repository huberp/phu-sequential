#include "PluginProcessor.h"
#include "HostTransport.h"
#include "ParameterLayout.h"
#include "PluginEditor.h"
#include "arpeggiator/ConfigurationBuilder.h"

PluginProcessor::PluginProcessor() : juce::AudioProcessor(
    juce::AudioProcessor::BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters_(*this, nullptr, "Parameters", phu::plugin::createParameterLayout()),
      parameterReader_(parameters_)
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
    const auto config = phu::arpeggiator::buildConfiguration(parameterReader_.read());
    engine_.process(midi, buffer.getNumSamples(), transport, sampleRate_, bypassed, config);
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

juce::AudioProcessorParameter* PluginProcessor::getBypassParameter() const
{
    return parameters_.getParameter(juce::String(phu::plugin::parameter_ids::bypass.data()));
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
