#pragma once

#include <array>
#include <cstdint>
#include "velocity/VelocityMode.h"
#include "Constants.h"

namespace phu::sequencing {

struct StepData {
    bool enabled = true;
    std::array<std::int16_t, velocityModeCount> valueByMode{};

    friend constexpr bool operator==(const StepData&, const StepData&) noexcept = default;
};

[[nodiscard]] constexpr StepData defaultStepData() noexcept {
    StepData data;
    data.enabled = true;
    data.valueByMode[static_cast<int>(VelocityMode::incoming)] = 0;
    data.valueByMode[static_cast<int>(VelocityMode::byStep)] = constants::velocity::defaultByStepValue;
    data.valueByMode[static_cast<int>(VelocityMode::deltaByStep)] = constants::velocity::defaultDeltaValue;
    data.valueByMode[static_cast<int>(VelocityMode::relative)] = constants::velocity::defaultPercentValue;
    return data;
}

class StepPattern {
public:
    StepPattern() noexcept;

    [[nodiscard]] const StepData& step(int index) const noexcept;

    void setEnabled(int index, bool enabled) noexcept;

    void setValue(int index, VelocityMode mode, int value) noexcept;

    void applyStepCountChange(int oldCount, int newCount) noexcept;

    friend bool operator==(const StepPattern&, const StepPattern&) noexcept = default;

private:
    std::array<StepData, constants::lanes::maximumStepCount> steps_;
};

}  // namespace phu::sequencing
