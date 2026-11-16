#include "PluginProcessor.h"
#include "HostTransport.h"
#include "ParameterLayout.h"
#include "PluginEditor.h"
#include "StateSerializer.h"
#include "arpeggiator/ConfigurationBuilder.h"

PluginProcessor::PluginProcessor() : juce::AudioProcessor(
    juce::AudioProcessor::BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters_(*this, nullptr, "Parameters", phu::plugin::createParameterLayout()),
      parameterReader_(parameters_),
      presetManager_(parameters_, patternStore_)
{
    for (auto& step : currentStepIndex_) {
        step.store(-1, std::memory_order_relaxed);
    }
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

    const auto status = engine_.core().playbackStatus();
    for (std::size_t lane = 0; lane < currentStepIndex_.size(); ++lane) {
        const auto& laneStatus = status.lanes[lane];
        currentStepIndex_[lane].store(laneStatus.isRunning ? laneStatus.currentStepIndex : -1,
                                      std::memory_order_relaxed);
    }
}

int PluginProcessor::currentStepIndex(int laneIndex) const noexcept
{
    if (laneIndex < 0 || laneIndex >= static_cast<int>(currentStepIndex_.size())) {
        return -1;
    }
    return currentStepIndex_[static_cast<std::size_t>(laneIndex)].load(std::memory_order_relaxed);
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

void PluginProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    phu::plugin::writeStateBlock(phu::plugin::captureState(parameters_, patternStore_), destData);
}

void PluginProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    const auto result = phu::plugin::readStateBlock(data, sizeInBytes);
    phu::plugin::applyState(result.data, parameters_, patternStore_);
}

// JUCE plugin wrapper macro
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}
