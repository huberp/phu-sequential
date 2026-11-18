#pragma once

#include "arpeggiator/LaneState.h"
#include "sequencing/Command.h"

namespace phu::arpeggiator {

// Implemented by the adapter over PatternStore + APVTS (D-55); keeps commands JUCE-free. [MT]
class LaneStateAccess {
public:
    virtual ~LaneStateAccess() = default;
    [[nodiscard]] virtual LaneState get(int laneIndex) const = 0;
    virtual void set(int laneIndex, const LaneState&) = 0;
};

// Command modifying one lane (sec. 23.3). execute() captures the lane as memento, applies doExecute() and
// writes the result back; undo() writes the memento back. [MT]
class LaneCommand : public sequencing::Command {
public:
    LaneCommand(LaneStateAccess& access, int laneIndex);

    void execute() final;
    void undo() final;

protected:
    virtual void doExecute(LaneState& lane) = 0;

    [[nodiscard]] int laneIndex() const noexcept { return laneIndex_; }

private:
    LaneStateAccess& access_;
    int laneIndex_;
    LaneState savedState_;
};

}  // namespace phu::arpeggiator
