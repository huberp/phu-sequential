#include "PresetManager.h"

#include <algorithm>

namespace phu::plugin {

namespace {

juce::String sanitizedFileName(const juce::String& name) {
    const auto cleaned = juce::File::createLegalFileName(name.trim()).trim();
    return cleaned.replaceCharacters("./\\", "___");
}

}  // namespace

PresetManager::PresetManager(juce::AudioProcessorValueTreeState& apvts, PatternStore& store)
    : PresetManager(apvts, store, userPresetDirectory()) {}

PresetManager::PresetManager(juce::AudioProcessorValueTreeState& apvts, PatternStore& store,
                             juce::File userPresetFolder)
    : apvts_(apvts), store_(store), folder_(std::move(userPresetFolder)) {}

juce::File PresetManager::userPresetDirectory() {
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("phu")
        .getChildFile("phu-sequential")
        .getChildFile("Presets");
}

std::vector<PresetInfo> PresetManager::listPresets() const {
    std::vector<PresetInfo> presets;
    if (!folder_.isDirectory()) {
        return presets;
    }
    for (const auto& entry : juce::RangedDirectoryIterator(folder_, false, "*" + presetExtension, juce::File::findFiles)) {
        const auto file = entry.getFile();
        if (file.hasFileExtension(presetExtension)) {
            presets.push_back(PresetInfo{file.getFileNameWithoutExtension(), false, -1, file});
        }
    }
    std::sort(presets.begin(), presets.end(), [](const PresetInfo& a, const PresetInfo& b) {
        const int order = a.name.compareIgnoreCase(b.name);
        return order != 0 ? order < 0 : a.name < b.name;
    });
    return presets;
}

LoadOutcome PresetManager::loadPreset(const PresetInfo& preset) {
    if (!preset.file.existsAsFile()) {
        return LoadOutcome::rejectedCorrupt;
    }
    const auto result = fromXmlText(preset.file.loadFileAsString());
    applyState(result.data, apvts_, store_);
    currentName_ = preset.name;
    if (onStateLoaded) {
        onStateLoaded();
    }
    return result.outcome;
}

bool PresetManager::saveUserPreset(const juce::String& name) {
    const auto fileName = sanitizedFileName(name);
    if (fileName.isEmpty() || !folder_.createDirectory().wasOk()) {
        return false;
    }
    const auto file = folder_.getChildFile(fileName + presetExtension);
    const auto xml = toXmlText(captureState(apvts_, store_), StateKind::preset, name.trim());
    if (!file.replaceWithText(xml)) {
        return false;
    }
    currentName_ = name.trim();
    return true;
}

LoadOutcome PresetManager::step(int direction) {
    const auto presets = listPresets();
    if (presets.empty()) {
        return LoadOutcome::rejectedCorrupt;
    }
    const auto count = static_cast<int>(presets.size());
    int current = -1;
    for (int i = 0; i < count; ++i) {
        if (presets[static_cast<std::size_t>(i)].name == currentName_) {
            current = i;
            break;
        }
    }
    const int next = current < 0 ? (direction > 0 ? 0 : count - 1) : (current + direction + count) % count;
    return loadPreset(presets[static_cast<std::size_t>(next)]);
}

LoadOutcome PresetManager::loadNext() {
    return step(1);
}

LoadOutcome PresetManager::loadPrevious() {
    return step(-1);
}

}  // namespace phu::plugin
