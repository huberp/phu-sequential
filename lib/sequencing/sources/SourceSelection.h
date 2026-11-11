#pragma once
#include <cstdint>
#include <optional>
#include <string_view>
#include "../HeldNoteTracker.h"
#include "../Constants.h"

namespace phu::sequencing {

enum class InputMode : std::uint8_t {
    chordFollow = 0,
    channel = 1
};

inline constexpr int inputModeCount = 2;

struct SourceSelector {
    InputMode inputMode = InputMode::chordFollow;
    int followIndex = constants::lanes::defaultFollowIndex;
    int followChannel = constants::lanes::defaultFollowChannel;
    int chordInputChannel = constants::lanes::defaultChordInputChannel;
    friend constexpr bool operator==(const SourceSelector&, const SourceSelector&) noexcept = default;
};

using SourceNote = HeldNote;

using SelectSourceFunction = std::optional<SourceNote> (*)(const HeldNoteTracker&, const SourceSelector&) noexcept;

struct InputModeDefinition {
    std::string_view name;
    std::string_view persistenceKey;
    SelectSourceFunction select;
};

[[nodiscard]] std::optional<SourceNote> selectSource(const HeldNoteTracker&, const SourceSelector&) noexcept;

enum class SourceChange : std::uint8_t {
    unchanged,
    started,
    pitchChanged,
    released
};

[[nodiscard]] constexpr SourceChange classifySourceChange(std::optional<SourceNote> previous,
                                                          std::optional<SourceNote> current) noexcept;

// Implementation of constexpr function
constexpr SourceChange classifySourceChange(std::optional<SourceNote> previous,
                                            std::optional<SourceNote> current) noexcept {
    if (!previous.has_value() && !current.has_value()) {
        return SourceChange::unchanged;
    }
    if (!previous.has_value() && current.has_value()) {
        return SourceChange::started;
    }
    if (previous.has_value() && !current.has_value()) {
        return SourceChange::released;
    }
    // Both have values
    if (previous->pitch != current->pitch) {
        return SourceChange::pitchChanged;
    }
    return SourceChange::unchanged;
}

}  // namespace phu::sequencing
