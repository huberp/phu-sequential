#include <catch2/catch_test_macros.hpp>
#include "sequencing/TextGridParser.h"
#include "sequencing/NoteValues.h"
#include "sequencing/Constants.h"

// M05 Text-grid parser: parsing comma-separated note value list (§5.2, §5.3, D-8).
using namespace phu::sequencing;

TEST_CASE("TextGrid: activeEntries returns span of the valid entries",
          "[M05][text-grid]") {
    TextGrid grid;
    grid.entryCount = 3;
    grid.entries[0] = NoteLength{8, Subdivision::triplet};
    grid.entries[1] = NoteLength{8, Subdivision::dotted};
    grid.entries[2] = NoteLength{4, Subdivision::straight};
    const auto active = grid.activeEntries();
    REQUIRE(active.size() == 3);
    CHECK(active[0] == NoteLength{8, Subdivision::triplet});
    CHECK(active[1] == NoteLength{8, Subdivision::dotted});
    CHECK(active[2] == NoteLength{4, Subdivision::straight});
}

TEST_CASE("TextGrid: operator== compares entryCount and first entryCount entries only",
          "[M05][text-grid]") {
    TextGrid grid1, grid2;
    grid1.entryCount = 2;
    grid1.entries[0] = NoteLength{8, Subdivision::straight};
    grid1.entries[1] = NoteLength{4, Subdivision::straight};
    grid1.entries[2] = NoteLength{1, Subdivision::straight};  // should be ignored
    
    grid2.entryCount = 2;
    grid2.entries[0] = NoteLength{8, Subdivision::straight};
    grid2.entries[1] = NoteLength{4, Subdivision::straight};
    grid2.entries[2] = NoteLength{2, Subdivision::straight};  // different but ignored
    
    CHECK(grid1 == grid2);
    
    grid2.entryCount = 3;  // now the third entry matters
    CHECK_FALSE(grid1 == grid2);
}

TEST_CASE("TextGridParser: valid single entry parses correctly",
          "[M05][text-grid][REQ-5.2-valid]") {
    const auto result = parseTextGrid("1/8");
    REQUIRE(result.isValid());
    REQUIRE(result.grid.has_value());
    CHECK(result.errors.empty());
    CHECK(result.grid->entryCount == 1);
    CHECK(result.grid->entries[0] == NoteLength{8, Subdivision::straight});
}

TEST_CASE("TextGridParser: valid multiple entries parse correctly",
          "[M05][text-grid][REQ-5.2-valid]") {
    const auto result = parseTextGrid("1/8, 1/4, 1/16");
    REQUIRE(result.isValid());
    REQUIRE(result.grid.has_value());
    CHECK(result.errors.empty());
    CHECK(result.grid->entryCount == 3);
    CHECK(result.grid->entries[0] == NoteLength{8, Subdivision::straight});
    CHECK(result.grid->entries[1] == NoteLength{4, Subdivision::straight});
    CHECK(result.grid->entries[2] == NoteLength{16, Subdivision::straight});
}

TEST_CASE("TextGridParser: example from spec '1/8t, 1/8., 1/4' parses with correct ticks",
          "[M05][text-grid][REQ-5.2-4]") {
    const auto result = parseTextGrid("1/8t, 1/8., 1/4");
    REQUIRE(result.isValid());
    REQUIRE(result.grid.has_value());
    CHECK(result.errors.empty());
    CHECK(result.grid->entryCount == 3);
    
    CHECK(result.grid->entries[0].denominator == 8);
    CHECK(result.grid->entries[0].subdivision == Subdivision::triplet);
    CHECK(noteLengthTicks(result.grid->entries[0]) == 320);  // 1/8 triplet
    
    CHECK(result.grid->entries[1].denominator == 8);
    CHECK(result.grid->entries[1].subdivision == Subdivision::dotted);
    CHECK(noteLengthTicks(result.grid->entries[1]) == 720);  // 1/8 dotted
    
    CHECK(result.grid->entries[2].denominator == 4);
    CHECK(result.grid->entries[2].subdivision == Subdivision::straight);
    CHECK(noteLengthTicks(result.grid->entries[2]) == 960);  // 1/4 straight
}

TEST_CASE("TextGridParser: all 7 valid denominators parse correctly",
          "[M05][text-grid][REQ-5.2-5]") {
    const std::array<int, 7> validDenominators{1, 2, 4, 8, 16, 32, 64};
    for (int den : validDenominators) {
        const auto text = "1/" + std::to_string(den);
        const auto result = parseTextGrid(text);
        CAPTURE(text);
        REQUIRE(result.isValid());
        CHECK(result.grid->entryCount == 1);
        CHECK(result.grid->entries[0].denominator == den);
    }
}

TEST_CASE("TextGridParser: triplet suffix 't' parsed correctly",
          "[M05][text-grid][REQ-5.2-6]") {
    const auto result = parseTextGrid("1/8t");
    REQUIRE(result.isValid());
    CHECK(result.grid->entries[0].subdivision == Subdivision::triplet);
}

TEST_CASE("TextGridParser: dotted suffix '.' parsed correctly",
          "[M05][text-grid][REQ-5.2-6]") {
    const auto result = parseTextGrid("1/8.");
    REQUIRE(result.isValid());
    CHECK(result.grid->entries[0].subdivision == Subdivision::dotted);
}

TEST_CASE("TextGridParser: no suffix yields straight subdivision",
          "[M05][text-grid]") {
    const auto result = parseTextGrid("1/8");
    REQUIRE(result.isValid());
    CHECK(result.grid->entries[0].subdivision == Subdivision::straight);
}

TEST_CASE("TextGridParser: whitespace around entries is tolerated",
          "[M05][text-grid][REQ-5.2-9]") {
    const auto result = parseTextGrid("  1/8  ,  1/4  ");
    REQUIRE(result.isValid());
    CHECK(result.grid->entryCount == 2);
    CHECK(result.grid->entries[0] == NoteLength{8, Subdivision::straight});
    CHECK(result.grid->entries[1] == NoteLength{4, Subdivision::straight});
}

TEST_CASE("TextGridParser: tabs and spaces are treated as whitespace",
          "[M05][text-grid][REQ-5.2-9]") {
    const auto result = parseTextGrid("\t1/8\t,\t1/4\t");
    REQUIRE(result.isValid());
    CHECK(result.grid->entryCount == 2);
}

TEST_CASE("TextGridParser: mixed tabs and spaces around entry",
          "[M05][text-grid][REQ-5.2-9]") {
    const auto result = parseTextGrid(" \t 1/8 \t , 1/4");
    REQUIRE(result.isValid());
    CHECK(result.grid->entryCount == 2);
}

TEST_CASE("TextGridParser: inner whitespace in entry like '1 /8' is invalid",
          "[M05][text-grid]") {
    const auto result = parseTextGrid("1 /8");
    REQUIRE_FALSE(result.isValid());
    REQUIRE(result.errors.size() == 1);
    CHECK(result.errors[0].entryIndex == 0);
    CHECK(result.errors[0].reason == TextGridErrorReason::malformedEntry);
}

TEST_CASE("TextGridParser: space within fraction '1/ 8' is invalid",
          "[M05][text-grid]") {
    const auto result = parseTextGrid("1/ 8");
    REQUIRE_FALSE(result.isValid());
    REQUIRE(result.errors.size() == 1);
    CHECK(result.errors[0].entryIndex == 0);
    CHECK(result.errors[0].reason == TextGridErrorReason::malformedEntry);
}

TEST_CASE("TextGridParser: numerator '3/8' is invalid (not 1)",
          "[M05][text-grid][REQ-5.2-5]") {
    const auto result = parseTextGrid("3/8");
    REQUIRE_FALSE(result.isValid());
    REQUIRE(result.errors.size() == 1);
    CHECK(result.errors[0].entryIndex == 0);
    CHECK(result.errors[0].reason == TextGridErrorReason::invalidNumerator);
}

TEST_CASE("TextGridParser: numerator '2/4' is invalid",
          "[M05][text-grid][REQ-5.2-5]") {
    const auto result = parseTextGrid("2/4");
    REQUIRE_FALSE(result.isValid());
    REQUIRE(result.errors.size() == 1);
    CHECK(result.errors[0].entryIndex == 0);
    CHECK(result.errors[0].reason == TextGridErrorReason::invalidNumerator);
}

TEST_CASE("TextGridParser: numerator '0/4' is invalid",
          "[M05][text-grid][REQ-5.2-5]") {
    const auto result = parseTextGrid("0/4");
    REQUIRE_FALSE(result.isValid());
    REQUIRE(result.errors.size() == 1);
    CHECK(result.errors[0].entryIndex == 0);
    CHECK(result.errors[0].reason == TextGridErrorReason::invalidNumerator);
}

TEST_CASE("TextGridParser: leading zero '01/4' is invalid (not exactly '1')",
          "[M05][text-grid][REQ-5.2-5]") {
    const auto result = parseTextGrid("01/4");
    REQUIRE_FALSE(result.isValid());
    REQUIRE(result.errors.size() == 1);
    CHECK(result.errors[0].entryIndex == 0);
    CHECK(result.errors[0].reason == TextGridErrorReason::invalidNumerator);
}

TEST_CASE("TextGridParser: sign '+1/4' is invalid",
          "[M05][text-grid][REQ-5.2-5]") {
    const auto result = parseTextGrid("+1/4");
    REQUIRE_FALSE(result.isValid());
    REQUIRE(result.errors.size() == 1);
    CHECK(result.errors[0].entryIndex == 0);
    CHECK(result.errors[0].reason == TextGridErrorReason::invalidNumerator);
}

TEST_CASE("TextGridParser: denominator '1/3' is invalid (not in list)",
          "[M05][text-grid][REQ-5.2-5]") {
    const auto result = parseTextGrid("1/3");
    REQUIRE_FALSE(result.isValid());
    REQUIRE(result.errors.size() == 1);
    CHECK(result.errors[0].entryIndex == 0);
    CHECK(result.errors[0].reason == TextGridErrorReason::invalidDenominator);
}

TEST_CASE("TextGridParser: denominator '1/128' is invalid (too large)",
          "[M05][text-grid][REQ-5.2-5]") {
    const auto result = parseTextGrid("1/128");
    REQUIRE_FALSE(result.isValid());
    REQUIRE(result.errors.size() == 1);
    CHECK(result.errors[0].entryIndex == 0);
    CHECK(result.errors[0].reason == TextGridErrorReason::invalidDenominator);
}

TEST_CASE("TextGridParser: denominator '1/0' is invalid (division by zero)",
          "[M05][text-grid][REQ-5.2-5]") {
    const auto result = parseTextGrid("1/0");
    REQUIRE_FALSE(result.isValid());
    REQUIRE(result.errors.size() == 1);
    CHECK(result.errors[0].entryIndex == 0);
    CHECK(result.errors[0].reason == TextGridErrorReason::invalidDenominator);
}

TEST_CASE("TextGridParser: leading zero in denominator '1/04' is invalid",
          "[M05][text-grid][REQ-5.2-5]") {
    const auto result = parseTextGrid("1/04");
    REQUIRE_FALSE(result.isValid());
    REQUIRE(result.errors.size() == 1);
    CHECK(result.errors[0].entryIndex == 0);
    CHECK(result.errors[0].reason == TextGridErrorReason::invalidDenominator);
}

TEST_CASE("TextGridParser: both suffixes '1/8t.' is conflictingSuffixes error",
          "[M05][text-grid][REQ-5.2-6]") {
    const auto result = parseTextGrid("1/8t.");
    REQUIRE_FALSE(result.isValid());
    REQUIRE(result.errors.size() == 1);
    CHECK(result.errors[0].entryIndex == 0);
    CHECK(result.errors[0].reason == TextGridErrorReason::conflictingSuffixes);
}

TEST_CASE("TextGridParser: repeated suffix '1/8tt' is conflictingSuffixes error",
          "[M05][text-grid][REQ-5.2-6]") {
    const auto result = parseTextGrid("1/8tt");
    REQUIRE_FALSE(result.isValid());
    REQUIRE(result.errors.size() == 1);
    CHECK(result.errors[0].entryIndex == 0);
    CHECK(result.errors[0].reason == TextGridErrorReason::conflictingSuffixes);
}

TEST_CASE("TextGridParser: repeated suffix '1/8..' is conflictingSuffixes error",
          "[M05][text-grid][REQ-5.2-6]") {
    const auto result = parseTextGrid("1/8..");
    REQUIRE_FALSE(result.isValid());
    REQUIRE(result.errors.size() == 1);
    CHECK(result.errors[0].entryIndex == 0);
    CHECK(result.errors[0].reason == TextGridErrorReason::conflictingSuffixes);
}

TEST_CASE("TextGridParser: unknown suffix '1/8x' is rejected",
          "[M05][text-grid][REQ-5.2-6]") {
    const auto result = parseTextGrid("1/8x");
    REQUIRE_FALSE(result.isValid());
    REQUIRE(result.errors.size() == 1);
    CHECK(result.errors[0].entryIndex == 0);
    CHECK(result.errors[0].reason == TextGridErrorReason::unknownSuffix);
}

TEST_CASE("TextGridParser: uppercase suffix '1/8T' is invalid (lowercase only)",
          "[M05][text-grid][REQ-5.2-6]") {
    const auto result = parseTextGrid("1/8T");
    REQUIRE_FALSE(result.isValid());
    REQUIRE(result.errors.size() == 1);
    CHECK(result.errors[0].entryIndex == 0);
    CHECK(result.errors[0].reason == TextGridErrorReason::unknownSuffix);
}

TEST_CASE("TextGridParser: uppercase suffix '1/8D' is invalid",
          "[M05][text-grid]") {
    const auto result = parseTextGrid("1/8D");
    REQUIRE_FALSE(result.isValid());
    REQUIRE(result.errors.size() == 1);
    CHECK(result.errors[0].entryIndex == 0);
    CHECK(result.errors[0].reason == TextGridErrorReason::unknownSuffix);
}

TEST_CASE("TextGridParser: malformed 'abc' is rejected",
          "[M05][text-grid]") {
    const auto result = parseTextGrid("abc");
    REQUIRE_FALSE(result.isValid());
    REQUIRE(result.errors.size() == 1);
    CHECK(result.errors[0].entryIndex == 0);
    CHECK(result.errors[0].reason == TextGridErrorReason::malformedEntry);
}

TEST_CASE("TextGridParser: malformed '1/' is rejected (no denominator)",
          "[M05][text-grid]") {
    const auto result = parseTextGrid("1/");
    REQUIRE_FALSE(result.isValid());
    REQUIRE(result.errors.size() == 1);
    CHECK(result.errors[0].entryIndex == 0);
    CHECK(result.errors[0].reason == TextGridErrorReason::malformedEntry);
}

TEST_CASE("TextGridParser: malformed '/8' is rejected (no numerator)",
          "[M05][text-grid]") {
    const auto result = parseTextGrid("/8");
    REQUIRE_FALSE(result.isValid());
    REQUIRE(result.errors.size() == 1);
    CHECK(result.errors[0].entryIndex == 0);
    CHECK(result.errors[0].reason == TextGridErrorReason::malformedEntry);
}

TEST_CASE("TextGridParser: malformed '1' is rejected (no slash/denominator)",
          "[M05][text-grid]") {
    const auto result = parseTextGrid("1");
    REQUIRE_FALSE(result.isValid());
    REQUIRE(result.errors.size() == 1);
    CHECK(result.errors[0].entryIndex == 0);
    CHECK(result.errors[0].reason == TextGridErrorReason::malformedEntry);
}

TEST_CASE("TextGridParser: empty text '' is emptyText error at index 0",
          "[M05][text-grid][REQ-5.2-9]") {
    const auto result = parseTextGrid("");
    REQUIRE_FALSE(result.isValid());
    REQUIRE(result.errors.size() == 1);
    CHECK(result.errors[0].entryIndex == 0);
    CHECK(result.errors[0].reason == TextGridErrorReason::emptyText);
}

TEST_CASE("TextGridParser: whitespace-only text is emptyText error",
          "[M05][text-grid][REQ-5.2-9]") {
    const auto result = parseTextGrid("   \t   ");
    REQUIRE_FALSE(result.isValid());
    REQUIRE(result.errors.size() == 1);
    CHECK(result.errors[0].entryIndex == 0);
    CHECK(result.errors[0].reason == TextGridErrorReason::emptyText);
}

TEST_CASE("TextGridParser: empty entry from double comma '1/8,,1/4' reports error at index 1",
          "[M05][text-grid][REQ-5.2-9]") {
    const auto result = parseTextGrid("1/8,,1/4");
    REQUIRE_FALSE(result.isValid());
    REQUIRE(result.errors.size() >= 1);
    const auto emptyError = std::find_if(result.errors.begin(), result.errors.end(),
        [](const TextGridError& e) { return e.reason == TextGridErrorReason::emptyEntry; });
    REQUIRE(emptyError != result.errors.end());
    CHECK(emptyError->entryIndex == 1);
}

TEST_CASE("TextGridParser: trailing comma creates empty entry error",
          "[M05][text-grid][REQ-5.2-9]") {
    const auto result = parseTextGrid("1/8,");
    REQUIRE_FALSE(result.isValid());
    REQUIRE(result.errors.size() >= 1);
    const auto emptyError = std::find_if(result.errors.begin(), result.errors.end(),
        [](const TextGridError& e) { return e.reason == TextGridErrorReason::emptyEntry; });
    REQUIRE(emptyError != result.errors.end());
    CHECK(emptyError->entryIndex == 1);
}

TEST_CASE("TextGridParser: exactly 64 entries is valid",
          "[M05][text-grid][REQ-5.2-8]") {
    std::string text64;
    for (int i = 0; i < 64; ++i) {
        if (i > 0) text64 += ",";
        text64 += "1/8";
    }
    const auto result = parseTextGrid(text64);
    REQUIRE(result.isValid());
    CHECK(result.grid->entryCount == 64);
}

TEST_CASE("TextGridParser: 65 entries exceeds limit, last entry marked tooManyEntries",
          "[M05][text-grid][REQ-5.2-8]") {
    std::string text65;
    for (int i = 0; i < 65; ++i) {
        if (i > 0) text65 += ",";
        text65 += "1/8";
    }
    const auto result = parseTextGrid(text65);
    REQUIRE_FALSE(result.isValid());
    REQUIRE(result.errors.size() >= 1);
    const auto tooManyError = std::find_if(result.errors.begin(), result.errors.end(),
        [](const TextGridError& e) { return e.reason == TextGridErrorReason::tooManyEntries; });
    REQUIRE(tooManyError != result.errors.end());
    CHECK(tooManyError->entryIndex == 64);
}

TEST_CASE("TextGridParser: all errors reported with ascending entryIndex",
          "[M05][text-grid]") {
    const auto result = parseTextGrid("1/8, 3/4, 1/8x, 1/2");
    REQUIRE_FALSE(result.isValid());
    REQUIRE(result.errors.size() >= 2);
    for (std::size_t i = 1; i < result.errors.size(); ++i) {
        CHECK(result.errors[i].entryIndex > result.errors[i - 1].entryIndex);
    }
}

TEST_CASE("TextGridParser: error textBegin/textEnd report byte range in original text",
          "[M05][text-grid]") {
    const std::string text = "1/8, 3/4";
    const auto result = parseTextGrid(text);
    REQUIRE_FALSE(result.isValid());
    REQUIRE(result.errors.size() >= 1);
    const auto invalidNum = std::find_if(result.errors.begin(), result.errors.end(),
        [](const TextGridError& e) { return e.reason == TextGridErrorReason::invalidNumerator && e.entryIndex == 1; });
    REQUIRE(invalidNum != result.errors.end());
    CHECK(invalidNum->textBegin >= 0);
    CHECK(invalidNum->textEnd > invalidNum->textBegin);
    CHECK(invalidNum->textEnd <= text.size());
    const auto errorText = text.substr(invalidNum->textBegin, invalidNum->textEnd - invalidNum->textBegin);
    CHECK(errorText.find("3") != std::string::npos);
}

TEST_CASE("TextGridParser: malformed entry at index 0 has textBegin/textEnd covering entry",
          "[M05][text-grid]") {
    const std::string text = "abc";
    const auto result = parseTextGrid(text);
    REQUIRE_FALSE(result.isValid());
    REQUIRE(result.errors.size() == 1);
    CHECK(result.errors[0].textBegin == 0);
    CHECK(result.errors[0].textEnd == text.size());
}

TEST_CASE("TextGridParser: malformed entry after comma has correct textBegin/textEnd",
          "[M05][text-grid]") {
    const std::string text = "1/8, xyz";
    const auto result = parseTextGrid(text);
    REQUIRE_FALSE(result.isValid());
    REQUIRE(result.errors.size() == 1);
    CHECK(result.errors[0].entryIndex == 1);
    CHECK(result.errors[0].textBegin > 0);
    CHECK(result.errors[0].textEnd > result.errors[0].textBegin);
}

TEST_CASE("TextGridParser: grid rejected as whole when any error present",
          "[M05][text-grid][REQ-5.3]") {
    const auto validResult = parseTextGrid("1/8, 1/4");
    REQUIRE(validResult.isValid());
    REQUIRE(validResult.grid.has_value());
    
    const auto invalidResult = parseTextGrid("1/8, 3/4");
    REQUIRE_FALSE(invalidResult.isValid());
    REQUIRE_FALSE(invalidResult.grid.has_value());
}

TEST_CASE("TextGridParser: mixed valid and invalid entries, all errors reported",
          "[M05][text-grid]") {
    const auto result = parseTextGrid("1/8, 3/4, 1/4, 1/8x");
    REQUIRE_FALSE(result.isValid());
    const auto reasons = std::vector<TextGridErrorReason>(result.errors.size());
    REQUIRE(result.errors.size() >= 2);
    CHECK(std::any_of(result.errors.begin(), result.errors.end(),
        [](const TextGridError& e) { return e.reason == TextGridErrorReason::invalidNumerator; }));
    CHECK(std::any_of(result.errors.begin(), result.errors.end(),
        [](const TextGridError& e) { return e.reason == TextGridErrorReason::unknownSuffix; }));
}

TEST_CASE("formatTextGrid: round-trip with canonical format",
          "[M05][text-grid][REQ-5.2-canonical]") {
    const std::string canonical = "1/8t, 1/8., 1/4";
    const auto parseResult = parseTextGrid(canonical);
    REQUIRE(parseResult.isValid());
    
    const auto formatted = formatTextGrid(*parseResult.grid);
    const auto reparsed = parseTextGrid(formatted);
    REQUIRE(reparsed.isValid());
    
    CHECK(*parseResult.grid == *reparsed.grid);
}

TEST_CASE("formatTextGrid: outputs comma-space-separated list",
          "[M05][text-grid]") {
    TextGrid grid;
    grid.entryCount = 2;
    grid.entries[0] = NoteLength{8, Subdivision::triplet};
    grid.entries[1] = NoteLength{4, Subdivision::straight};
    
    const auto formatted = formatTextGrid(grid);
    CHECK(formatted.find(", ") != std::string::npos);
    const auto parts = formatted.find(',');
    CHECK(parts != std::string::npos);
}

TEST_CASE("formatTextGrid: empty grid produces empty string",
          "[M05][text-grid]") {
    TextGrid grid;
    grid.entryCount = 0;
    
    const auto formatted = formatTextGrid(grid);
    CHECK(formatted.empty());
}

TEST_CASE("formatTextGrid: single entry without separator",
          "[M05][text-grid]") {
    TextGrid grid;
    grid.entryCount = 1;
    grid.entries[0] = NoteLength{8, Subdivision::straight};
    
    const auto formatted = formatTextGrid(grid);
    CHECK(formatted.find(',') == std::string::npos);
}

TEST_CASE("formatTextGrid: triplet suffix 't' in output",
          "[M05][text-grid]") {
    TextGrid grid;
    grid.entryCount = 1;
    grid.entries[0] = NoteLength{8, Subdivision::triplet};
    
    const auto formatted = formatTextGrid(grid);
    CHECK(formatted.find('t') != std::string::npos);
}

TEST_CASE("formatTextGrid: dotted suffix '.' in output",
          "[M05][text-grid]") {
    TextGrid grid;
    grid.entryCount = 1;
    grid.entries[0] = NoteLength{8, Subdivision::dotted};
    
    const auto formatted = formatTextGrid(grid);
    CHECK(formatted.find('.') != std::string::npos);
}

TEST_CASE("TextGridParser: entry with only leading whitespace parses as entry",
          "[M05][text-grid]") {
    const auto result = parseTextGrid("1/8,   1/4");
    REQUIRE(result.isValid());
    CHECK(result.grid->entryCount == 2);
}

TEST_CASE("TextGridParser: entry with only trailing whitespace parses as entry",
          "[M05][text-grid]") {
    const auto result = parseTextGrid("1/8   , 1/4");
    REQUIRE(result.isValid());
    CHECK(result.grid->entryCount == 2);
}

TEST_CASE("TextGridParser: single entry with no surrounding whitespace",
          "[M05][text-grid]") {
    const auto result = parseTextGrid("1/8");
    REQUIRE(result.isValid());
    CHECK(result.grid->entryCount == 1);
}

TEST_CASE("TextGridParser: valid denominator edge case 1/1 (whole note)",
          "[M05][text-grid][REQ-5.2-5]") {
    const auto result = parseTextGrid("1/1");
    REQUIRE(result.isValid());
    CHECK(result.grid->entries[0].denominator == 1);
}

TEST_CASE("TextGridParser: valid denominator edge case 1/64 (smallest fraction)",
          "[M05][text-grid][REQ-5.2-5]") {
    const auto result = parseTextGrid("1/64");
    REQUIRE(result.isValid());
    CHECK(result.grid->entries[0].denominator == 64);
}

TEST_CASE("TextGridParser: all seven valid denominators in single grid",
          "[M05][text-grid][REQ-5.2-5]") {
    const auto result = parseTextGrid("1/1, 1/2, 1/4, 1/8, 1/16, 1/32, 1/64");
    REQUIRE(result.isValid());
    CHECK(result.grid->entryCount == 7);
    const std::array<int, 7> expectedDenominators{1, 2, 4, 8, 16, 32, 64};
    for (std::size_t i = 0; i < 7; ++i) {
        CHECK(result.grid->entries[i].denominator == expectedDenominators[i]);
    }
}
