#pragma once

#include "BlockEngine.h"
#include "ParameterReader.h"
#include "PatternStore.h"
#include "PresetManager.h"
#include "sequencing/Constants.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <atomic>

class PluginProcessor : public juce::AudioProcessor
{
public:
    PluginProcessor();
    ~PluginProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    // [RT]
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    // [RT]
    void processBlockBypassed(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;
    using juce::AudioProcessor::processBlockBypassed;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    juce::AudioProcessorParameter* getBypassParameter() const override;
    juce::AudioProcessorValueTreeState& parameters() noexcept { return parameters_; }
    phu::plugin::PatternStore& patternStore() noexcept { return patternStore_; }
    phu::plugin::PresetManager& presetManager() noexcept { return presetManager_; }

    // Wait-free playback position for the editor: step index currently sounding in the lane, -1 if not running.
    [[nodiscard]] int currentStepIndex(int laneIndex) const noexcept;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

private:
    juce::AudioProcessorValueTreeState parameters_;
    phu::plugin::ParameterReader parameterReader_;
    phu::plugin::PatternStore patternStore_;
    phu::plugin::PresetManager presetManager_;
    phu::plugin::BlockEngine engine_;
    std::array<std::atomic<int>, phu::sequencing::constants::lanes::laneCount> currentStepIndex_{};
    double sampleRate_ = 44100.0;

    void processInternal(juce::AudioBuffer<float>&, juce::MidiBuffer&, bool bypassed);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginProcessor)
};
