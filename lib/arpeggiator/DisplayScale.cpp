#include "arpeggiator/DisplayScale.h"
#include <algorithm>
#include <limits>

namespace phu::arpeggiator {

DisplayScale computeDisplayScale(
    std::span<const sequencing::GridLayout, sequencing::constants::lanes::laneCount> layouts,
    double availableWidthPixels) noexcept {
    DisplayScale scale;

    sequencing::Beats longestLoopBeats = 0.0;
    sequencing::Beats shortestStepBeats = std::numeric_limits<sequencing::Beats>::max();
    bool anyStep = false;

    for (const auto& layout : layouts) {
        if (layout.isEmpty()) {
            continue;
        }
        longestLoopBeats = std::max(longestLoopBeats, layout.loopLengthBeats());
        for (int i = 0; i < layout.stepCount; ++i) {
            const auto stepBeats = sequencing::ticksToBeats(layout.stepLengthTicks[static_cast<std::size_t>(i)]);
            shortestStepBeats = std::min(shortestStepBeats, stepBeats);
            anyStep = true;
        }
    }

    double pixelsPerBeat = (availableWidthPixels > 0.0 && longestLoopBeats > 0.0)
        ? (availableWidthPixels / longestLoopBeats)
        : 0.0;

    if (anyStep && shortestStepBeats > 0.0) {
        const double minimumPixelsPerBeat =
            static_cast<double>(sequencing::constants::editor::minimumStepDisplayWidth) / shortestStepBeats;
        pixelsPerBeat = std::max(pixelsPerBeat, minimumPixelsPerBeat);
    }

    scale.pixelsPerBeat = pixelsPerBeat;
    scale.contentWidthPixels = longestLoopBeats * pixelsPerBeat;

    for (std::size_t laneIndex = 0; laneIndex < layouts.size(); ++laneIndex) {
        const auto& layout = layouts[laneIndex];
        auto& geometry = scale.lanes[laneIndex];
        geometry.stepCount = layout.stepCount;
        geometry.loopWidthPixels = layout.loopLengthBeats() * pixelsPerBeat;
        for (int i = 0; i <= layout.stepCount; ++i) {
            geometry.stepStartPixels[static_cast<std::size_t>(i)] =
                sequencing::ticksToBeats(layout.stepStartTicks[static_cast<std::size_t>(i)]) * pixelsPerBeat;
        }
    }

    return scale;
}

} // namespace phu::arpeggiator
