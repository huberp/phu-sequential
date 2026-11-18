#include "PluginEditor.h"

#include "PluginProcessor.h"
#include "arpeggiator/ConfigurationBuilder.h"
#include "arpeggiator/LaneConfiguration.h"
#include "editor/EditorLayout.h"
#include "sequencing/StepTiming.h"

#include <algorithm>

namespace layout = phu::plugin::editor::layout;
namespace editor_constants = phu::sequencing::constants::editor;
using phu::sequencing::constants::lanes::laneCount;

PluginEditor::PluginEditor(PluginProcessor& processor)
    : juce::AudioProcessorEditor(&processor), processorRef(processor), parameterReader_(processor.parameters()),
      globalBar_(processor.parameters(), processor.presetManager(), processor.laneEditHistory())
{
    setLookAndFeel(&lookAndFeel_);
    setWantsKeyboardFocus(true);
    addAndMakeVisible(globalBar_);

    for (int lane = 0; lane < laneCount; ++lane) {
        auto row = std::make_unique<phu::plugin::editor::LaneRow>(processor.parameters(), processor.laneEditHistory(), lane);
        laneContainer_.addAndMakeVisible(*row);
        laneRows_[static_cast<std::size_t>(lane)] = std::move(row);
    }
    laneViewport_.setViewedComponent(&laneContainer_, false);
    laneViewport_.setScrollBarsShown(true, false);
    addAndMakeVisible(laneViewport_);

    stepAreaScrollBar_.setAutoHide(false);
    stepAreaScrollBar_.addListener(this);
    addChildComponent(stepAreaScrollBar_);

    setResizable(true, true);
    setResizeLimits(editor_constants::minimumWidth, editor_constants::minimumHeight, editor_constants::maximumWidth,
                    editor_constants::maximumHeight);
    setSize(editor_constants::defaultWidth, editor_constants::defaultHeight);

    processor.patternStore().addChangeListener(this);
    startTimerHz(editor_constants::statusRefreshHz);
}

PluginEditor::~PluginEditor()
{
    stopTimer();
    processorRef.patternStore().removeChangeListener(this);
    stepAreaScrollBar_.removeListener(this);
    setLookAndFeel(nullptr);
}

bool PluginEditor::keyPressed(const juce::KeyPress& key)
{
    const auto cmd = juce::ModifierKeys::commandModifier;
    const auto shift = juce::ModifierKeys::shiftModifier;
    auto& history = processorRef.laneEditHistory();
    if (key == juce::KeyPress('z', cmd, 0)) {
        history.undo();
        return true;
    }
    if (key == juce::KeyPress('z', cmd | shift, 0) || key == juce::KeyPress('y', cmd, 0)) {
        history.redo();
        return true;
    }
    return false;
}

void PluginEditor::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}

void PluginEditor::resized()
{
    auto area = getLocalBounds();
    globalBar_.setBounds(area.removeFromTop(editor_constants::globalBarHeight));

    auto scrollBarArea = area.removeFromBottom(layout::scrollBarThickness);
    laneViewport_.setBounds(area);

    const int rowWidth = laneViewport_.getMaximumVisibleWidth();
    laneContainer_.setSize(rowWidth, layout::laneContainerHeight());
    for (int lane = 0; lane < laneCount; ++lane) {
        laneRows_[static_cast<std::size_t>(lane)]->setBounds(
            0, lane * editor_constants::laneRowHeight, rowWidth, editor_constants::laneRowHeight);
    }

    const auto stepArea = layout::computeLaneRowRegions({0, 0, rowWidth, editor_constants::laneRowHeight}).stepArea;
    stepAreaScrollBar_.setBounds(scrollBarArea.withX(stepArea.getX()).withWidth(stepArea.getWidth()));
    refresh();
}

void PluginEditor::changeListenerCallback(juce::ChangeBroadcaster*)
{
    documentDirty_ = true;
}

void PluginEditor::timerCallback()
{
    globalBar_.refreshUndoState();
    refresh();
    for (int lane = 0; lane < laneCount; ++lane) {
        laneRows_[static_cast<std::size_t>(lane)]->setCurrentStep(processorRef.currentStepIndex(lane));
    }
}

void PluginEditor::scrollBarMoved(juce::ScrollBar*, double newRangeStart)
{
    for (auto& row : laneRows_) {
        row->setScrollOffset(static_cast<int>(newRangeStart));
    }
}

// Parameters change without notification to the editor, so the lane view state is rebuilt on every tick.
void PluginEditor::refresh()
{
    if (documentDirty_) {
        document_ = processorRef.patternStore().copyDocument();
        documentDirty_ = false;
    }
    const auto configuration = phu::arpeggiator::buildConfiguration(parameterReader_.read());

    std::array<phu::sequencing::GridLayout, laneCount> layouts;
    for (int lane = 0; lane < laneCount; ++lane) {
        const auto index = static_cast<std::size_t>(lane);
        layouts[index] = phu::arpeggiator::makeActiveGridLayout(
            configuration.lanes[index], document_.lane(lane).pattern.appliedTextGrid);
    }

    const int stepAreaWidth = layout::stepAreaWidth(laneViewport_.getMaximumVisibleWidth());
    scale_ = phu::arpeggiator::computeDisplayScale(layouts, static_cast<double>(stepAreaWidth));
    for (int lane = 0; lane < laneCount; ++lane) {
        const auto index = static_cast<std::size_t>(lane);
        laneRows_[index]->update(configuration.lanes[index], document_.lane(lane), scale_.lanes[index]);
    }
    updateScrollBar(stepAreaWidth);
}

void PluginEditor::updateScrollBar(int stepAreaWidth)
{
    const double content = std::max(scale_.contentWidthPixels, static_cast<double>(stepAreaWidth));
    const bool needed = scale_.contentWidthPixels > stepAreaWidth;
    stepAreaScrollBar_.setVisible(needed);
    stepAreaScrollBar_.setRangeLimits(0.0, content);
    stepAreaScrollBar_.setCurrentRange(needed ? stepAreaScrollBar_.getCurrentRangeStart() : 0.0,
                                       static_cast<double>(stepAreaWidth));
}
