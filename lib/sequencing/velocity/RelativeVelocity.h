#pragma once

namespace phu::sequencing {

[[nodiscard]] constexpr int relativeVelocity(int stepValue, int inputVelocity) noexcept {
    return (inputVelocity * stepValue + 50) / 100;
}

}  // namespace phu::sequencing
