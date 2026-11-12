#include "StepData.h"
#include "velocity/VelocityModeRegistry.h"
#include "Constants.h"

namespace phu::sequencing {

using namespace constants;

StepPattern::StepPattern() noexcept {
    for (int i = 0; i < lanes::maximumStepCount; ++i) {
        steps_[i] = defaultStepData();
    }
}

const StepData& StepPattern::step(int index) const noexcept {
    if (index < 0) {
        index = 0;
    }
    if (index >= lanes::maximumStepCount) {
        index = lanes::maximumStepCount - 1;
    }
    return steps_[index];
}

void StepPattern::setEnabled(int index, bool enabled) noexcept {
    if (index < 0 || index >= lanes::maximumStepCount) {
        return;
    }
    steps_[index].enabled = enabled;
}

void StepPattern::setValue(int index, VelocityMode mode, int value) noexcept {
    if (index < 0 || index >= lanes::maximumStepCount) {
        return;
    }
    if (mode == VelocityMode::incoming) {
        return;
    }
    
    const int clamped = clampStepValue(mode, value);
    steps_[index].valueByMode[static_cast<int>(mode)] = static_cast<std::int16_t>(clamped);
}

void StepPattern::applyStepCountChange(int oldCount, int newCount) noexcept {
    (void)oldCount;  // unused
    
    if (newCount < 0) {
        newCount = 0;
    }
    if (newCount > lanes::maximumStepCount) {
        newCount = lanes::maximumStepCount;
    }
    
    for (int i = newCount; i < lanes::maximumStepCount; ++i) {
        steps_[i] = defaultStepData();
    }
}

}  // namespace phu::sequencing
