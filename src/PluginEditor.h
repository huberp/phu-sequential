#pragma once

#include "ParameterReader.h"
#include "arpeggiator/DisplayScale.h"
#include "arpeggiator/PatternDocument.h"
#include "editor/EditorLookAndFeel.h"
#include "editor/GlobalBar.h"
#include "editor/LaneRow.h"
#include "sequencing/Constants.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <memory>

class PluginProcessor;

// Message thread only (sec. 14, 17). Parameters via attachments; pattern data only via PatternStore.
class PluginEditor : public juce::AudioProcessorEditor,
                     private juce::Timer,
                     private juce::ChangeListener,
                     private juce::ScrollBar::Listener
{
public:
    explicit PluginEditor(PluginProcessor& processor);
    ~PluginEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress&) override;

private:
    void timerCallback() override;
    void changeListenerCallback(juce::ChangeBroadcaster*) override;
    void scrollBarMoved(juce::ScrollBar*, double newRangeStart) override;

    void refresh();
    void updateScrollBar(int stepAreaWidth);

    PluginProcessor& processorRef;
    phu::plugin::ParameterReader parameterReader_;
    phu::plugin::editor::EditorLookAndFeel lookAndFeel_;
    phu::plugin::editor::GlobalBar globalBar_;
    juce::Viewport laneViewport_;  // vertical scrolling of the lane rows
    juce::Component laneContainer_;
    std::array<std::unique_ptr<phu::plugin::editor::LaneRow>, phu::sequencing::constants::lanes::laneCount> laneRows_;
    juce::ScrollBar stepAreaScrollBar_{false};  // one horizontal bar shared by all step areas
    phu::arpeggiator::PatternDocument document_;
    phu::arpeggiator::DisplayScale scale_;
    bool documentDirty_ = true;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginEditor)
};
