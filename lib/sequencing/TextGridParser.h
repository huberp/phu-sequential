#pragma once

#include "sequencing/NoteValues.h"
#include "sequencing/Constants.h"
#include <optional>
#include <vector>
#include <span>
#include <string>
#include <string_view>
#include <array>
#include <cstdint>

namespace phu::sequencing {

struct TextGrid {                                    // a VALID grid only
    int entryCount = 0;                              // 0..64; 0 = "no grid" (D-9)
    std::array<NoteLength, constants::timing::textGridMaximumEntries> entries{};
    [[nodiscard]] std::span<const NoteLength> activeEntries() const noexcept;
    friend bool operator==(const TextGrid&, const TextGrid&) noexcept; // first entryCount entries
};

enum class TextGridErrorReason : std::uint8_t {
    emptyText,           // whole text empty / whitespace only
    emptyEntry,          // ",," or trailing ","
    malformedEntry,      // not "<num>/<den>[suffix]"
    invalidNumerator,    // numerator token != "1"
    invalidDenominator,  // not one of 1,2,4,8,16,32,64
    unknownSuffix,       // suffix other than 't' or '.'
    conflictingSuffixes, // both / repeated suffixes, e.g. "1/8t." or "1/8tt"
    tooManyEntries       // entry index >= 64
};

struct TextGridError {
    int entryIndex;                 // 0-based entry index (0 for emptyText)
    std::size_t textBegin;          // byte range in the original text, for red marking (§5.3)
    std::size_t textEnd;            // exclusive
    TextGridErrorReason reason;
};

struct TextGridParseResult {
    std::optional<TextGrid> grid;          // set iff errors.empty()
    std::vector<TextGridError> errors;     // every invalid entry, ascending entryIndex
    [[nodiscard]] bool isValid() const noexcept { return grid.has_value(); }
};

[[nodiscard]] TextGridParseResult parseTextGrid(std::string_view text);   // [MT]
[[nodiscard]] std::string formatTextGrid(const TextGrid&);                // [MT] canonical "1/8t, 1/8., 1/4"

} // namespace phu::sequencing
