#pragma once

#include "PatternStore.h"
#include "arpeggiator/LaneCommandRegistry.h"
#include "arpeggiator/LaneCommands.h"
#include "sequencing/UndoManager.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <functional>
#include <memory>
#include <string>

namespace phu::plugin {

// Adapter side of M25/M27 (D-55, D-56): LaneStateAccess over APVTS + PatternStore, the undo manager and the
// command registry. All editor edits of lane state go through execute()/executeCoalesced(). Message thread only,
// no audio-thread path (sec. 23.8). [MT]
class LaneEditHistory final : public arpeggiator::LaneStateAccess {
public:
    LaneEditHistory(juce::AudioProcessorValueTreeState&, PatternStore&);

    [[nodiscard]] arpeggiator::LaneState get(int laneIndex) const override;
    void set(int laneIndex, const arpeggiator::LaneState&) override;

    // Runs `makeCommand` with this object as the LaneStateAccess and pushes the result onto the undo stack.
    void execute(const std::function<std::unique_ptr<arpeggiator::LaneCommand>(arpeggiator::LaneStateAccess&)>& makeCommand);
    // Creates through the registry; returns false for an unknown command name.
    bool execute(const std::string& name, int laneIndex, const arpeggiator::CommandArguments& = {});

    // Repeated edits with the same key (drag, typing, colour selection) collapse into one undo step until any other
    // edit, undo/redo or endGesture().
    void executeCoalesced(const std::string& key,
                          const std::function<std::unique_ptr<arpeggiator::LaneCommand>(arpeggiator::LaneStateAccess&)>& makeCommand);
    void endGesture() noexcept { coalesceKey_.clear(); }

    void undo();
    void redo();
    // After state/preset load the history is dropped (A-47).
    void clear();

    [[nodiscard]] bool canUndo() const noexcept { return undoManager_.canUndo(); }
    [[nodiscard]] bool canRedo() const noexcept { return undoManager_.canRedo(); }
    [[nodiscard]] arpeggiator::LaneCommandRegistry& registry() noexcept { return registry_; }
    [[nodiscard]] sequencing::UndoManager& undoManager() noexcept { return undoManager_; }

private:
    juce::AudioProcessorValueTreeState& apvts_;
    PatternStore& store_;
    sequencing::UndoManager undoManager_;
    arpeggiator::LaneCommandRegistry registry_;
    std::string coalesceKey_;
};

}  // namespace phu::plugin
