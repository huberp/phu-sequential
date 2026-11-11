#include "ChordFollowSource.h"
#include "../HeldNoteTracker.h"
#include "SourceSelection.h"
#include "../Constants.h"

namespace phu::sequencing {

std::optional<SourceNote> selectChordFollowSource(const HeldNoteTracker& tracker, const SourceSelector& selector) noexcept {
    // Clamp followIndex to valid range [0, maximumFollowIndex]
    const int clampedFollowIndex = (selector.followIndex < constants::lanes::minimumFollowIndex) 
        ? constants::lanes::minimumFollowIndex
        : (selector.followIndex > constants::lanes::maximumFollowIndex) 
        ? constants::lanes::maximumFollowIndex
        : selector.followIndex;

    return tracker.heldNoteAtIndex(selector.chordInputChannel, clampedFollowIndex);
}

}  // namespace phu::sequencing
