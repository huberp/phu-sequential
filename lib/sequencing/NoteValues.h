#pragma once

#include "sequencing/Constants.h"
#include "sequencing/MusicalTime.h"
#include <array>
#include <cstdint>
#include <span>
#include <string_view>

// Data-driven musical note values (§5.1, §16.2). This is the only place
// mapping subdivision names <-> rational factors, and base-timing index <->
// denominator.
namespace phu::sequencing {

enum class Subdivision : std::uint8_t { straight = 0, triplet = 1, dotted = 2 };
inline constexpr int subdivisionCount = 3;

struct SubdivisionDefinition {
    std::string_view name;            // "Straight" / "Triplet" / "Dotted" (parameter choice text)
    std::string_view persistenceKey;  // "straight" / "triplet" / "dotted" (state XML)
    constants::timing::RationalFactor factor;
    char textGridSuffix;              // '\0', 't', '.'
};

inline constexpr std::array<SubdivisionDefinition, subdivisionCount> subdivisionRegistry{{
    {"Straight", "straight", constants::timing::straightFactor, '\0'},
    {"Triplet",  "triplet",  constants::timing::tripletFactor,  constants::timing::textGridTripletSuffix},
    {"Dotted",   "dotted",   constants::timing::dottedFactor,   constants::timing::textGridDottedSuffix},
}};

struct NoteLength {                  // "1/denominator" modified by subdivision
    int denominator = 16;            // power of two, 1..64
    Subdivision subdivision = Subdivision::straight;
    friend constexpr bool operator==(const NoteLength&, const NoteLength&) noexcept = default;
};

// ticksPerWholeNote / denominator * factor.numerator / factor.denominator (exact integer)
[[nodiscard]] constexpr TickCount noteLengthTicks(NoteLength length) noexcept {
    const auto& factor = subdivisionRegistry[static_cast<std::size_t>(length.subdivision)].factor;
    const TickCount straightTicks = constants::timing::ticksPerWholeNote / length.denominator;
    return (straightTicks * factor.numerator) / factor.denominator;
}

struct BaseTimingDefinition { std::string_view name; int denominator; };  // name "1/16"

namespace detail {
// Built from constants::timing::baseTimingDenominators; index = parameter choice index.
inline constexpr std::array<BaseTimingDefinition, 6> baseTimingDefinitions{{
    {"1/32", 32}, {"1/16", 16}, {"1/8", 8}, {"1/4", 4}, {"1/2", 2}, {"1/1", 1},
}};
} // namespace detail

[[nodiscard]] constexpr std::span<const BaseTimingDefinition> baseTimingRegistry() noexcept {
    return std::span<const BaseTimingDefinition>(detail::baseTimingDefinitions);
}

// index clamped to the valid registry range.
[[nodiscard]] constexpr NoteLength baseTimingNoteLength(int baseTimingIndex, Subdivision subdivision) noexcept {
    const auto registry = baseTimingRegistry();
    const int lastIndex = static_cast<int>(registry.size()) - 1;
    const int clampedIndex = baseTimingIndex < 0 ? 0 : (baseTimingIndex > lastIndex ? lastIndex : baseTimingIndex);
    return NoteLength{registry[static_cast<std::size_t>(clampedIndex)].denominator, subdivision};
}

} // namespace phu::sequencing
