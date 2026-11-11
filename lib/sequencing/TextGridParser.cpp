#include "sequencing/TextGridParser.h"
#include <cctype>
#include <algorithm>
#include <stdexcept>

namespace phu::sequencing {

std::span<const NoteLength> TextGrid::activeEntries() const noexcept {
    return std::span<const NoteLength>(entries.data(), static_cast<std::size_t>(entryCount));
}

bool operator==(const TextGrid& lhs, const TextGrid& rhs) noexcept {
    if (lhs.entryCount != rhs.entryCount) {
        return false;
    }
    for (int i = 0; i < lhs.entryCount; ++i) {
        if (lhs.entries[static_cast<std::size_t>(i)] != rhs.entries[static_cast<std::size_t>(i)]) {
            return false;
        }
    }
    return true;
}

namespace {

bool isWhitespace(char c) noexcept {
    return c == ' ' || c == '\t';
}

std::string_view trimWhitespace(std::string_view s) noexcept {
    std::size_t start = 0;
    while (start < s.size() && isWhitespace(s[start])) {
        ++start;
    }
    if (start >= s.size()) {
        return std::string_view{s.data() + start, 0};
    }
    std::size_t end = s.size();
    while (end > start && isWhitespace(s[end - 1])) {
        --end;
    }
    return std::string_view{s.data() + start, end - start};
}

bool isDenominatorValid(int den) noexcept {
    for (int validDen : constants::timing::textGridDenominators) {
        if (den == validDen) {
            return true;
        }
    }
    return false;
}

bool hasNoLeadingZero(std::string_view s) noexcept {
    if (s.empty()) return false;
    if (s.size() > 1 && s[0] == '0') return false;
    return true;
}

} // namespace

TextGridParseResult parseTextGrid(std::string_view text) {
    TextGridParseResult result;
    std::vector<TextGridError> errors;

    std::string_view trimmedText = trimWhitespace(text);
    
    if (trimmedText.empty()) {
        result.grid = std::nullopt;
        result.errors.push_back({0, 0, text.size(), TextGridErrorReason::emptyText});
        return result;
    }

    TextGrid grid;
    grid.entryCount = 0;
    int entryIndex = 0;
    std::size_t pos = 0;
    bool hadTrailingComma = false;

    while (pos < text.size() || hadTrailingComma) {
        hadTrailingComma = false;

        std::size_t entryStart = pos;

        std::size_t commaPos = text.find(constants::timing::textGridEntrySeparator, pos);
        std::string_view rawEntry;
        
        if (commaPos == std::string_view::npos) {
            rawEntry = text.substr(pos);
            pos = text.size();
        } else {
            rawEntry = text.substr(pos, commaPos - pos);
            pos = commaPos + 1;
            hadTrailingComma = (commaPos == text.size() - 1);
        }

        std::string_view entry = trimWhitespace(rawEntry);
        std::size_t entryEnd = entryStart + rawEntry.size();

        if (entry.empty()) {
            errors.push_back({entryIndex, entryStart, entryEnd, TextGridErrorReason::emptyEntry});
            ++entryIndex;
            continue;
        }

        if (entryIndex >= constants::timing::textGridMaximumEntries) {
            errors.push_back({entryIndex, entryStart, entryEnd, TextGridErrorReason::tooManyEntries});
            ++entryIndex;
            continue;
        }

        std::size_t slashPos = entry.find(constants::timing::textGridFractionSeparator);
        if (slashPos == std::string_view::npos || slashPos == 0 || slashPos == entry.size() - 1) {
            errors.push_back({entryIndex, entryStart, entryEnd, TextGridErrorReason::malformedEntry});
            ++entryIndex;
            continue;
        }

        std::string_view numStr = entry.substr(0, slashPos);
        std::string_view denAndSuffix = entry.substr(slashPos + 1);

        // Check for internal whitespace in the fraction part
        bool hasInternalWhitespace = false;
        for (char c : numStr) {
            if (isWhitespace(c)) {
                hasInternalWhitespace = true;
                break;
            }
        }
        if (!hasInternalWhitespace) {
            for (char c : denAndSuffix) {
                if (c != constants::timing::textGridTripletSuffix && 
                    c != constants::timing::textGridDottedSuffix &&
                    !std::isdigit(static_cast<unsigned char>(c))) {
                    if (isWhitespace(c)) {
                        hasInternalWhitespace = true;
                    }
                }
            }
        }

        if (hasInternalWhitespace) {
            errors.push_back({entryIndex, entryStart, entryEnd, TextGridErrorReason::malformedEntry});
            ++entryIndex;
            continue;
        }

        if (numStr != "1") {
            errors.push_back({entryIndex, entryStart, entryEnd, TextGridErrorReason::invalidNumerator});
            ++entryIndex;
            continue;
        }

        int den = 0;
        std::size_t denEndPos = 0;
        Subdivision subdivision = Subdivision::straight;

        for (std::size_t i = 0; i < denAndSuffix.size(); ++i) {
            if (std::isdigit(static_cast<unsigned char>(denAndSuffix[i]))) {
                denEndPos = i + 1;
            } else {
                break;
            }
        }

        if (denEndPos == 0) {
            errors.push_back({entryIndex, entryStart, entryEnd, TextGridErrorReason::malformedEntry});
            ++entryIndex;
            continue;
        }

        std::string_view denStr = denAndSuffix.substr(0, denEndPos);
        std::string_view suffixPart = denAndSuffix.substr(denEndPos);

        if (!hasNoLeadingZero(denStr)) {
            errors.push_back({entryIndex, entryStart, entryEnd, TextGridErrorReason::invalidDenominator});
            ++entryIndex;
            continue;
        }

        try {
            std::size_t idx = 0;
            den = std::stoi(std::string(denStr), &idx);
            if (idx != denStr.size()) {
                throw std::invalid_argument("incomplete parse");
            }
        } catch (...) {
            errors.push_back({entryIndex, entryStart, entryEnd, TextGridErrorReason::invalidDenominator});
            ++entryIndex;
            continue;
        }

        if (!isDenominatorValid(den)) {
            errors.push_back({entryIndex, entryStart, entryEnd, TextGridErrorReason::invalidDenominator});
            ++entryIndex;
            continue;
        }

        bool hasTriplet = false;
        bool hasDotted = false;

        for (char c : suffixPart) {
            if (c == constants::timing::textGridTripletSuffix) {
                if (hasTriplet || hasDotted) {
                    errors.push_back({entryIndex, entryStart, entryEnd, TextGridErrorReason::conflictingSuffixes});
                    goto nextEntry;
                }
                hasTriplet = true;
                subdivision = Subdivision::triplet;
            } else if (c == constants::timing::textGridDottedSuffix) {
                if (hasDotted || hasTriplet) {
                    errors.push_back({entryIndex, entryStart, entryEnd, TextGridErrorReason::conflictingSuffixes});
                    goto nextEntry;
                }
                hasDotted = true;
                subdivision = Subdivision::dotted;
            } else {
                errors.push_back({entryIndex, entryStart, entryEnd, TextGridErrorReason::unknownSuffix});
                goto nextEntry;
            }
        }

        grid.entries[static_cast<std::size_t>(entryIndex)] = NoteLength{den, subdivision};
        ++grid.entryCount;

        nextEntry:
        ++entryIndex;
    }

    if (errors.empty()) {
        result.grid = grid;
    } else {
        result.grid = std::nullopt;
        result.errors = std::move(errors);
    }

    return result;
}

std::string formatTextGrid(const TextGrid& grid) {
    if (grid.entryCount == 0) {
        return "";
    }

    std::string result;
    for (int i = 0; i < grid.entryCount; ++i) {
        if (i > 0) {
            result += constants::timing::textGridEntrySeparator;
            result += ' ';
        }

        const auto& entry = grid.entries[static_cast<std::size_t>(i)];
        result += std::to_string(constants::timing::textGridNumerator);
        result += constants::timing::textGridFractionSeparator;
        result += std::to_string(entry.denominator);

        const auto& subdivDef = subdivisionRegistry[static_cast<std::size_t>(entry.subdivision)];
        if (subdivDef.textGridSuffix != '\0') {
            result += subdivDef.textGridSuffix;
        }
    }

    return result;
}

} // namespace phu::sequencing
