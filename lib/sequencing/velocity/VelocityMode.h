#pragma once

#include <cstdint>
#include <string_view>

namespace phu::sequencing {

enum class VelocityMode : std::uint8_t { incoming = 0, byStep = 1, deltaByStep = 2, relative = 3 };

inline constexpr int velocityModeCount = 4;

struct StepValueRange {
    int minimum;
    int maximum;
    int defaultValue;
};

using ComputeVelocityFunction = int (*)(int stepValue, int inputVelocity) noexcept;

struct VelocityModeDefinition {
    std::string_view name;
    std::string_view persistenceKey;
    bool usesStepValues;
    StepValueRange stepValueRange;
    ComputeVelocityFunction compute;
};

}  // namespace phu::sequencing
