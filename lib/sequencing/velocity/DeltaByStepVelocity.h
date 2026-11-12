#pragma once

namespace phu::sequencing {

[[nodiscard]] constexpr int deltaByStepVelocity(int stepValue, int inputVelocity) noexcept {
    return inputVelocity + stepValue;
}

}  // namespace phu::sequencing
