#pragma once

#include "MidiConversion.h"
#include "arpeggiator/SequencerCore.h"
#include "arpeggiator/SequencerConfiguration.h"
#include "arpeggiator/TransportTracker.h"
#include "sequencing/Constants.h"
#include "sequencing/OutputEventBuffer.h"
#include "sequencing/TripleBuffer.h"

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <bitset>
#include <cstdint>

namespace phu::plugin {

// Extracts the arch processInternal steps 0, 2-5 (D-30/D-31/D-45/D-51/D-53/D-54) out of
// PluginProcessor so they are testable without the plugin wrapper; state/parameters (step 1) are
// replaced by the `config` argument passed into process().
class BlockEngine {
public:
    BlockEngine() noexcept;

    void prepare() noexcept;

    // [RT]
    void process(juce::MidiBuffer& midi, int numSamples, const arpeggiator::TransportInfo&, double sampleRate,
                 bool bypassed, const arpeggiator::SequencerConfiguration& config) noexcept;

    [[nodiscard]] std::uint32_t droppedOutputEventsTotal() const noexcept;
    [[nodiscard]] std::uint32_t hostOutputGrowthCount() const noexcept;

    arpeggiator::SequencerCore& core() noexcept;

private:
    arpeggiator::SequencerCore core_;
    juce::MidiBuffer outputScratch_;
    std::bitset<sequencing::constants::capacity::maximumSoundingVoices> outputOpen_;
    DeferredInputStore deferredInput_;
    sequencing::OutputEventBuffer coreOutput_{sequencing::constants::capacity::outputEventsPerBlock};
    std::array<sequencing::MidiEvent, sequencing::constants::capacity::inputEventsPerChunk> inputChunk_{};
    std::uint32_t droppedOutputEventsTotal_ = 0;
    std::uint32_t hostOutputGrowthCount_ = 0;
    std::size_t knownHostCapacityBytes_ = 0;  // see note in BlockEngine.cpp
};

class ParameterReader;

// [RT] One plugin block exactly as PluginProcessor::processInternal runs it (arch step 1 + process()):
// parameters -> configuration, newly published patterns -> core, then BlockEngine::process. Shared by
// the processor and the host-trace replay (M28) so both stay identical.
void processPluginBlock(BlockEngine& engine, const ParameterReader& parameters,
                        sequencing::TripleBuffer<arpeggiator::PatternSnapshot>& patterns, juce::MidiBuffer& midi,
                        int numSamples, const arpeggiator::TransportInfo& transport, double sampleRate,
                        bool bypassed) noexcept;

}  // namespace phu::plugin
