#pragma once

namespace phu::sequencing {

[[nodiscard]] constexpr int byStepVelocity(int stepValue, int inputVelocity) noexcept {
    (void)inputVelocity;  // unused
    return stepValue;
}

}  // namespace phu::sequencing
