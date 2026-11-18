#pragma once

#include "StateSerializer.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <functional>
#include <vector>

namespace phu::plugin {

struct PresetInfo {
    juce::String name;
    bool isFactory = false;
    int factoryIndex = -1;  // into the factory list (none embedded yet)
    juce::File file;        // user presets
};

// Preset browser backend (sec. 13): factory presets first, then user presets from the user folder. [MT]
class PresetManager {
public:
    inline static const juce::String presetExtension{".phuseq"};

    PresetManager(juce::AudioProcessorValueTreeState&, PatternStore&);
    PresetManager(juce::AudioProcessorValueTreeState&, PatternStore&, juce::File userPresetFolder);

    // userApplicationDataDirectory/phu/phu-sequential/Presets
    [[nodiscard]] static juce::File userPresetDirectory();

    // Factory (fixed order), then user presets (*.phuseq) in case-insensitive name order.
    [[nodiscard]] std::vector<PresetInfo> listPresets() const;

    // A rejected preset applies the default state, exactly like a rejected host-state load.
    LoadOutcome loadPreset(const PresetInfo&);
    bool saveUserPreset(const juce::String& name);  // overwrites a preset of the same name
    LoadOutcome loadNext();                          // wraps around
    LoadOutcome loadPrevious();

    // Invoked after a preset replaced the state (A-47: the undo history is cleared).
    std::function<void()> onStateLoaded;

    [[nodiscard]] juce::String currentPresetName() const { return currentName_; }

private:
    LoadOutcome step(int direction);

    juce::AudioProcessorValueTreeState& apvts_;
    PatternStore& store_;
    juce::File folder_;
    juce::String currentName_;
};

}  // namespace phu::plugin
