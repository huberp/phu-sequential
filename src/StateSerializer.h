#pragma once

#include "LaneStateSerializer.h"
#include "PatternStore.h"
#include "StateMigration.h"
#include "arpeggiator/PatternDocument.h"
#include "arpeggiator/SequencerConfiguration.h"
#include "sequencing/Constants.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <cstdint>
#include <span>

namespace phu::plugin {

enum class StateKind : std::uint8_t { state, preset };

enum class LoadOutcome : std::uint8_t { loaded, migrated, loadedWithClamping, rejectedNewerVersion, rejectedCorrupt };

// JUCE-free payload of the complete plugin state (sec. 13).
struct PluginStateData {
    arpeggiator::SequencerConfiguration parameters = arpeggiator::defaultSequencerConfiguration();
    arpeggiator::PatternDocument document;
};

// data holds the sec. 20 default state when the load was rejected.
struct LoadResult {
    LoadOutcome outcome = LoadOutcome::rejectedCorrupt;
    PluginStateData data;
};

[[nodiscard]] juce::ValueTree toValueTree(const PluginStateData&, StateKind, const juce::String& presetName = {});

// Never throws. A migrated tree that also needed clamping reports loadedWithClamping.
[[nodiscard]] LoadResult fromValueTree(
    const juce::ValueTree&, std::span<const MigrationFunction> chain = productionMigrations(),
    int currentVersion = sequencing::constants::state::currentFormatVersion);

[[nodiscard]] juce::String toXmlText(const PluginStateData&, StateKind, const juce::String& presetName = {});
[[nodiscard]] LoadResult fromXmlText(const juce::String&);

// Host state: binary wrapper around the XML (AudioProcessor::copyXmlToBinary).
void writeStateBlock(const PluginStateData&, juce::MemoryBlock&);
[[nodiscard]] LoadResult readStateBlock(const void* data, int sizeInBytes);

[[nodiscard]] PluginStateData captureState(juce::AudioProcessorValueTreeState&, const PatternStore&);
void applyState(const PluginStateData&, juce::AudioProcessorValueTreeState&, PatternStore&);

// Single-lane counterparts (D-55): parameters are written through APVTS with host notification.
[[nodiscard]] arpeggiator::LaneState captureLaneState(juce::AudioProcessorValueTreeState&, const PatternStore&,
                                                      int laneIndex);
void applyLaneState(const arpeggiator::LaneState&, int laneIndex, juce::AudioProcessorValueTreeState&, PatternStore&);

}  // namespace phu::plugin
