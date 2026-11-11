#include "ChannelSource.h"
#include "../HeldNoteTracker.h"
#include "SourceSelection.h"

namespace phu::sequencing {

std::optional<SourceNote> selectChannelSource(const HeldNoteTracker& tracker, const SourceSelector& selector) noexcept {
    return tracker.mostRecentNote(selector.followChannel);
}

}  // namespace phu::sequencing
