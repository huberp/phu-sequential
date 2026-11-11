#include "SourceSelection.h"
#include "InputModeRegistry.h"
#include "../HeldNoteTracker.h"
#include "../Constants.h"

namespace phu::sequencing {

std::optional<SourceNote> selectSource(const HeldNoteTracker& tracker, const SourceSelector& selector) noexcept {
    // Clamp inputMode to valid range [0, inputModeCount-1]
    const int modeIndex = (selector.inputMode < static_cast<InputMode>(0))
        ? 0
        : (selector.inputMode >= static_cast<InputMode>(inputModeCount))
        ? inputModeCount - 1
        : static_cast<int>(selector.inputMode);

    // Registry lookup (no switch)
    return inputModeRegistry[modeIndex].select(tracker, selector);
}

}  // namespace phu::sequencing
