#include "editor/StepDisplay.h"

#include "editor/EditorLayout.h"
#include "editor/EditorLookAndFeel.h"
#include "sequencing/velocity/VelocityModeRegistry.h"

#include <algorithm>
#include <cmath>

namespace phu::plugin::editor {

namespace {

constexpr float cornerSize = 2.0f;

const sequencing::VelocityModeDefinition& definitionOf(sequencing::VelocityMode mode)
{
    return sequencing::velocityModeDefinition(mode);
}

}  // namespace

StepDisplay::StepDisplay(PatternStore& store, int laneIndex) : store_(store), laneIndex_(laneIndex) {}

void StepDisplay::setState(const StepDisplayState& state)
{
    if (state == state_) {
        return;
    }
    state_ = state;
    repaint();
}

void StepDisplay::setScrollOffset(int pixels)
{
    if (pixels != scrollOffset_) {
        scrollOffset_ = pixels;
        repaint();
    }
}

void StepDisplay::setCurrentStep(int stepIndex)
{
    if (stepIndex != currentStep_) {
        currentStep_ = stepIndex;
        repaint();
    }
}

bool StepDisplay::barsEnabled() const
{
    return definitionOf(state_.velocityMode).usesStepValues;
}

juce::Rectangle<int> StepDisplay::stepBounds(int step) const
{
    const auto& starts = state_.geometry.stepStartPixels;
    const auto index = static_cast<std::size_t>(step);
    const int left = static_cast<int>(std::lround(starts[index])) - scrollOffset_;
    const int right = static_cast<int>(std::lround(starts[index + 1])) - scrollOffset_;
    return {left, 0, right - left, getHeight()};
}

juce::Rectangle<int> StepDisplay::padBounds(int step) const
{
    return stepBounds(step).removeFromTop(layout::padHeight).reduced(1);
}

juce::Rectangle<int> StepDisplay::barBounds(int step) const
{
    auto bounds = stepBounds(step);
    bounds.removeFromTop(layout::padHeight + layout::gap / 2);
    return bounds.reduced(1, 0);
}

int StepDisplay::stepAt(int x) const
{
    for (int step = 0; step < state_.geometry.stepCount; ++step) {
        const auto bounds = stepBounds(step);
        if (x >= bounds.getX() && x < bounds.getRight()) {
            return step;
        }
    }
    return -1;
}

int StepDisplay::valueAt(int step, int y) const
{
    const auto bar = barBounds(step);
    const auto range = definitionOf(state_.velocityMode).stepValueRange;
    const double fraction = 1.0 - std::clamp(static_cast<double>(y - bar.getY()) / std::max(1, bar.getHeight()), 0.0, 1.0);
    return static_cast<int>(std::lround(range.minimum + fraction * (range.maximum - range.minimum)));
}

void StepDisplay::paint(juce::Graphics& g)
{
    const auto& definition = definitionOf(state_.velocityMode);
    const auto range = definition.stepValueRange;
    const bool bars = barsEnabled();
    const auto clip = g.getClipBounds();

    for (int step = 0; step < state_.geometry.stepCount; ++step) {
        const auto cell = stepBounds(step);
        if (cell.getRight() < clip.getX() || cell.getX() > clip.getRight()) {
            continue;
        }
        const auto& data = state_.pattern.step(step);
        const auto fill = state_.laneColour.withAlpha(state_.muted ? 0.4f : 1.0f);

        const auto pad = padBounds(step);
        g.setColour(data.enabled ? fill : EditorLookAndFeel::neutralStep());
        g.fillRoundedRectangle(pad.toFloat(), cornerSize);

        const auto bar = barBounds(step);
        g.setColour(EditorLookAndFeel::panel().withAlpha(bars ? 1.0f : 0.5f));
        g.fillRect(bar);
        if (bars) {
            const double span = std::max(1, range.maximum - range.minimum);
            const int zeroY = bar.getBottom() - static_cast<int>(std::lround((0 - range.minimum) / span * bar.getHeight()));
            const int valueY = bar.getBottom()
                - static_cast<int>(std::lround((data.valueByMode[static_cast<std::size_t>(state_.velocityMode)] - range.minimum) / span * bar.getHeight()));
            g.setColour(data.enabled ? fill : EditorLookAndFeel::neutralStep());
            g.fillRect(bar.getX(), std::min(zeroY, valueY), bar.getWidth(), std::max(1, std::abs(zeroY - valueY)));
            if (range.minimum < 0) {
                g.setColour(juce::Colours::white.withAlpha(0.3f));
                g.drawHorizontalLine(zeroY, static_cast<float>(bar.getX()), static_cast<float>(bar.getRight()));
            }
            if (step == draggedStep_) {
                g.setColour(juce::Colours::white);
                g.setFont(12.0f);
                g.drawText(juce::String(data.valueByMode[static_cast<std::size_t>(state_.velocityMode)]),
                           stepBounds(step).withTrimmedTop(layout::padHeight), juce::Justification::centredTop, false);
            }
        }

        if (step == currentStep_) {
            g.setColour(EditorLookAndFeel::amber());
            g.drawRect(cell.reduced(0), 2);
        }
    }
}

void StepDisplay::mouseDown(const juce::MouseEvent& event)
{
    const int step = stepAt(event.x);
    if (step < 0) {
        return;
    }
    if (event.y < layout::padHeight + layout::gap / 2) {
        store_.setStepEnabled(laneIndex_, step, !state_.pattern.step(step).enabled);
    } else if (barsEnabled()) {
        draggedStep_ = step;
        applyDrag(event.y);
    }
}

void StepDisplay::mouseDrag(const juce::MouseEvent& event)
{
    if (draggedStep_ >= 0) {
        applyDrag(event.y);
    }
}

void StepDisplay::mouseUp(const juce::MouseEvent&)
{
    draggedStep_ = -1;
    repaint();
}

void StepDisplay::applyDrag(int y)
{
    const int value = valueAt(draggedStep_, y);
    state_.pattern.setValue(draggedStep_, state_.velocityMode, value);  // immediate feedback; the store confirms
    store_.setStepValue(laneIndex_, draggedStep_, state_.velocityMode, value);
    repaint();
}

}  // namespace phu::plugin::editor
