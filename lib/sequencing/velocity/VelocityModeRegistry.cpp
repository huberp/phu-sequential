#include "VelocityModeRegistry.h"
#include "IncomingVelocity.h"
#include "ByStepVelocity.h"
#include "DeltaByStepVelocity.h"
#include "RelativeVelocity.h"
#include "../Constants.h"
#include <algorithm>

namespace phu::sequencing {

using namespace constants;

const std::array<VelocityModeDefinition, velocityModeCount> velocityModeRegistry{
    VelocityModeDefinition{"Incoming", "incoming", false, {0, 0, 0}, incomingVelocity},
    VelocityModeDefinition{"By Step", "byStep", true, {midi::lowestVelocity, midi::highestVelocity, velocity::defaultByStepValue}, byStepVelocity},
    VelocityModeDefinition{"Delta by Step", "deltaByStep", true, {velocity::minimumDelta, velocity::maximumDelta, velocity::defaultDeltaValue}, deltaByStepVelocity},
    VelocityModeDefinition{"Relative", "relative", true, {velocity::minimumPercent, velocity::maximumPercent, velocity::defaultPercentValue}, relativeVelocity},
};

const VelocityModeDefinition& velocityModeDefinition(VelocityMode mode) noexcept {
    const int index = static_cast<int>(mode);
    if (index >= 0 && index < velocityModeCount) {
        return velocityModeRegistry[index];
    }
    return velocityModeRegistry[static_cast<int>(VelocityMode::incoming)];
}

int clampStepValue(VelocityMode mode, int value) noexcept {
    const auto& def = velocityModeDefinition(mode);
    if (value < def.stepValueRange.minimum) {
        return def.stepValueRange.minimum;
    }
    if (value > def.stepValueRange.maximum) {
        return def.stepValueRange.maximum;
    }
    return value;
}

std::optional<std::uint8_t> computeOutputVelocity(VelocityMode mode, int stepValue, int inputVelocity) noexcept {
    const auto& def = velocityModeDefinition(mode);
    const int computed = def.compute(stepValue, inputVelocity);
    
    const int clamped = std::clamp(computed, midi::lowestVelocity, midi::highestVelocity);
    if (clamped == midi::lowestVelocity) {
        return std::nullopt;
    }

    return static_cast<std::uint8_t>(clamped);
}

}  // namespace phu::sequencing
