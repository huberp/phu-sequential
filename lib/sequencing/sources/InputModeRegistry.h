#pragma once
#include "SourceSelection.h"
#include "ChordFollowSource.h"
#include "ChannelSource.h"
#include <array>

namespace phu::sequencing {

inline constexpr std::array<InputModeDefinition, inputModeCount> inputModeRegistry{{
    {"Chord Follow", "chordFollow", &selectChordFollowSource},
    {"Channel",      "channel",     &selectChannelSource},
}};

}  // namespace phu::sequencing
