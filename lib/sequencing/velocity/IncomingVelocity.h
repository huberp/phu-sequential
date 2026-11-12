#pragma once

namespace phu::sequencing {

[[nodiscard]] constexpr int incomingVelocity(int stepValue, int inputVelocity) noexcept {
    (void)stepValue;  // unused
    return inputVelocity;
}

}  // namespace phu::sequencing
