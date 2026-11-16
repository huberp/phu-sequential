#include "editor/GlobalBar.h"

#include "ParameterIds.h"
#include "editor/EditorLayout.h"

#include <initializer_list>

namespace phu::plugin::editor {

GlobalBar::GlobalBar(juce::AudioProcessorValueTreeState& apvts, PresetManager& presets) : presets_(presets)
{
    thruAttachment_ = std::make_unique<juce::ButtonParameterAttachment>(
        *apvts.getParameter(juce::String(parameter_ids::thru.data())), thru_);
    consumeAttachment_ = std::make_unique<juce::ButtonParameterAttachment>(
        *apvts.getParameter(juce::String(parameter_ids::consume.data())), consume_);
    configureBarSlider(chordInputChannel_, "Chord Input Channel");
    channelAttachment_ = std::make_unique<juce::SliderParameterAttachment>(
        *apvts.getParameter(juce::String(parameter_ids::chordInputChannel.data())), chordInputChannel_);
    setBarSliderText(chordInputChannel_, "Chord Input Channel");

    presetName_.setJustificationType(juce::Justification::centred);
    presetName_.setColour(juce::Label::backgroundColourId, juce::Colours::black.withAlpha(0.25f));

    previous_.onClick = [this] { presets_.loadPrevious(); refreshPresetName(); };
    next_.onClick = [this] { presets_.loadNext(); refreshPresetName(); };
    save_.onClick = [this] { showSaveDialog(); };
    load_.onClick = [this] { showLoadMenu(); };

    for (auto* component : std::initializer_list<juce::Component*>{
             &thru_, &consume_, &chordInputChannel_, &presetName_, &previous_, &next_, &save_, &load_}) {
        addAndMakeVisible(*component);
    }
    refreshPresetName();
}

void GlobalBar::refreshPresetName()
{
    const auto name = presets_.currentPresetName();
    presetName_.setText(name.isEmpty() ? juce::String("(no preset)") : name, juce::dontSendNotification);
}

void GlobalBar::showLoadMenu()
{
    const auto list = presets_.listPresets();
    juce::PopupMenu menu;
    for (std::size_t i = 0; i < list.size(); ++i) {
        menu.addItem(static_cast<int>(i) + 1, list[i].isFactory ? "Factory: " + list[i].name : list[i].name);
    }
    if (list.empty()) {
        menu.addItem(-1, "(no presets)", false);
    }
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&load_),
                       [this, list, safe = juce::Component::SafePointer<GlobalBar>(this)](int result) {
                           if (safe != nullptr && result > 0) {
                               presets_.loadPreset(list[static_cast<std::size_t>(result) - 1]);
                               refreshPresetName();
                           }
                       });
}

void GlobalBar::showSaveDialog()
{
    auto* window = new juce::AlertWindow("Save preset", "Preset name:", juce::MessageBoxIconType::NoIcon, this);
    window->addTextEditor("name", presets_.currentPresetName());
    window->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey));
    window->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    window->enterModalState(true,
                            juce::ModalCallbackFunction::create(
                                [this, window, safe = juce::Component::SafePointer<GlobalBar>(this)](int result) {
                                    const auto name = window->getTextEditorContents("name").trim();
                                    if (safe != nullptr && result == 1 && name.isNotEmpty()) {
                                        presets_.saveUserPreset(name);
                                        refreshPresetName();
                                    }
                                }),
                            true);
}

void GlobalBar::resized()
{
    auto area = getLocalBounds().reduced(layout::margin);
    const int unit = layout::controlHeight;
    const auto row = [&](int width) { return area.removeFromLeft(width).withSizeKeepingCentre(width, unit); };
    thru_.setBounds(row(80));
    consume_.setBounds(row(100));
    area.removeFromLeft(layout::gap);
    chordInputChannel_.setBounds(row(200));
    area.removeFromLeft(layout::margin * 2);

    save_.setBounds(area.removeFromRight(70).withSizeKeepingCentre(70, unit));
    area.removeFromRight(layout::gap);
    load_.setBounds(area.removeFromRight(70).withSizeKeepingCentre(70, unit));
    area.removeFromRight(layout::gap);
    next_.setBounds(area.removeFromRight(unit * 2).withSizeKeepingCentre(unit * 2, unit));
    previous_.setBounds(area.removeFromRight(unit * 2).withSizeKeepingCentre(unit * 2, unit));
    area.removeFromRight(layout::gap);
    presetName_.setBounds(area.withSizeKeepingCentre(area.getWidth(), unit));
}

}  // namespace phu::plugin::editor
