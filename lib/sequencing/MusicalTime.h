#pragma once

#include "Constants.h"
#include <cstdint>

namespace phu::sequencing {

using Beats     = double;        // quarter-note beats (PPQ)
using TickCount = std::int64_t;  // constants::timing::ticksPerBeat per beat

[[nodiscard]] constexpr Beats ticksToBeats(TickCount ticks) noexcept {
    return ticks / static_cast<double>(constants::timing::ticksPerBeat);
}

[[nodiscard]] constexpr TickCount wholeBeatsToTicks(std::int64_t beats) noexcept {
    return beats * constants::timing::ticksPerBeat;
}

}  // namespace phu::sequencing
