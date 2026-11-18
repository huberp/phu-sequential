#include "editor/LaneRow.h"

#include "editor/EditorLayout.h"
#include "editor/EditorLookAndFeel.h"

namespace phu::plugin::editor {

LaneRow::LaneRow(juce::AudioProcessorValueTreeState& apvts, LaneEditHistory& edits, int laneIndex)
    : laneIndex_(laneIndex), strip_(apvts, edits, laneIndex, *this), textGrid_(edits, laneIndex),
      stepDisplay_(edits, laneIndex)
{
    addAndMakeVisible(textGrid_);
    addAndMakeVisible(stepDisplay_);
}

void LaneRow::update(const arpeggiator::LaneParameters& parameters, const arpeggiator::LaneDocument& document,
                     const arpeggiator::LaneDisplayGeometry& geometry)
{
    const auto colour = juce::Colour(document.color.red, document.color.green, document.color.blue);
    if (colour != laneColour_) {
        laneColour_ = colour;
        repaint();
    }
    strip_.update(parameters, colour);
    textGrid_.setText(document.textGridText);
    stepDisplay_.setState({geometry, document.pattern.steps, parameters.velocityMode, colour, parameters.mute});
}

void LaneRow::paint(juce::Graphics& g)
{
    const auto regions = layout::computeLaneRowRegions(getLocalBounds());
    g.setColour(EditorLookAndFeel::panel().withAlpha(0.35f));
    g.fillRect(getLocalBounds().withTrimmedBottom(1));

    const auto badge = regions.sidebar.withSize(layout::controlHeight, layout::controlHeight);
    g.setColour(laneColour_);
    g.fillRoundedRectangle(badge.toFloat(), 3.0f);
    g.setColour(juce::Colours::black);
    g.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    g.drawText(juce::String(laneIndex_ + 1), badge, juce::Justification::centred, false);
}

void LaneRow::resized()
{
    const auto regions = layout::computeLaneRowRegions(getLocalBounds());
    strip_.layOut(regions);
    textGrid_.setBounds(regions.textGrid);
    stepDisplay_.setBounds(regions.stepArea);
}

}  // namespace phu::plugin::editor
