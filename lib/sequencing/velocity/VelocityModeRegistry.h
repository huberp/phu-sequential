#pragma once

#include <array>
#include <optional>
#include <cstdint>
#include "VelocityMode.h"
#include "../Constants.h"

namespace phu::sequencing {

extern const std::array<VelocityModeDefinition, velocityModeCount> velocityModeRegistry;

[[nodiscard]] const VelocityModeDefinition& velocityModeDefinition(VelocityMode mode) noexcept;

[[nodiscard]] int clampStepValue(VelocityMode mode, int value) noexcept;

[[nodiscard]] std::optional<std::uint8_t> computeOutputVelocity(VelocityMode mode, int stepValue, int inputVelocity) noexcept;

}  // namespace phu::sequencing
