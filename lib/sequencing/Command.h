#pragma once

#include <string>

namespace phu::sequencing {

// Undoable operation (sec. 23.2). JUCE-free; message thread only. [MT]
class Command {
public:
    virtual ~Command() = default;
    virtual void execute() = 0;  // also used for redo
    virtual void undo() = 0;
    [[nodiscard]] virtual std::string getName() const = 0;
};

}  // namespace phu::sequencing
