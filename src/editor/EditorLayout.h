#pragma once

#include "sequencing/Constants.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <numeric>
#include <vector>

// Central place for all editor geometry (sec. 14). Sizes that are shared with the architecture live in
// sequencing::constants::editor; everything else derives from them, so changing the lane count or the lane row
// height never needs a change anywhere else.
namespace phu::plugin::editor::layout {

namespace constants = sequencing::constants;

inline constexpr int margin = 6;
inline constexpr int gap = 4;
inline constexpr int controlHeight = 22;
inline constexpr int textGridHeight = 22;
inline constexpr int scrollBarThickness = 14;
inline constexpr int padHeight = 18;
inline constexpr int minimumBarHeight = 12;

struct LaneRowRegions {
    juce::Rectangle<int> sidebar;     // badge + color picker, mute/latch, timing source switch, lane commands
    juce::Rectangle<int> controlRow1; // input mode, follow, output channel, transpose, gate
    juce::Rectangle<int> controlRow2; // base timing, subdivision, swing, step count, velocity mode
    juce::Rectangle<int> textGrid;
    juce::Rectangle<int> stepArea;
};

// rowBounds is the lane row in its own coordinates.
[[nodiscard]] inline LaneRowRegions computeLaneRowRegions(juce::Rectangle<int> rowBounds)
{
    LaneRowRegions regions;
    auto area = rowBounds.reduced(margin, gap / 2);
    regions.sidebar = area.removeFromLeft(constants::editor::controlStripWidth - margin);
    area.removeFromLeft(margin);
    regions.controlRow1 = area.removeFromTop(controlHeight);
    area.removeFromTop(gap);
    regions.controlRow2 = area.removeFromTop(controlHeight);
    area.removeFromTop(gap);
    regions.textGrid = area.removeFromTop(textGridHeight);
    area.removeFromTop(gap);
    regions.stepArea = area;
    return regions;
}

// Width of the step area (and text grid) of every lane for a given lane row width.
[[nodiscard]] inline int stepAreaWidth(int laneRowWidth)
{
    return computeLaneRowRegions({0, 0, laneRowWidth, constants::editor::laneRowHeight}).stepArea.getWidth();
}

[[nodiscard]] inline int laneContainerHeight()
{
    return constants::lanes::laneCount * constants::editor::laneRowHeight;
}

// Splits bounds into horizontal cells proportional to the weights, separated by `gap`.
[[nodiscard]] inline std::vector<juce::Rectangle<int>> splitHorizontally(
    juce::Rectangle<int> bounds, const std::vector<int>& weights)
{
    std::vector<juce::Rectangle<int>> cells;
    const int count = static_cast<int>(weights.size());
    const int totalWeight = std::accumulate(weights.begin(), weights.end(), 0);
    const int usable = bounds.getWidth() - gap * (count - 1);
    int x = bounds.getX();
    for (int i = 0; i < count; ++i) {
        const int width = i + 1 == count ? bounds.getRight() - x : usable * weights[static_cast<std::size_t>(i)] / totalWeight;
        cells.emplace_back(x, bounds.getY(), width, bounds.getHeight());
        x += width + gap;
    }
    return cells;
}

}  // namespace phu::plugin::editor::layout
