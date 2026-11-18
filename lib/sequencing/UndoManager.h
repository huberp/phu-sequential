#pragma once

#include "sequencing/Command.h"
#include "sequencing/Constants.h"

#include <cstddef>
#include <deque>
#include <memory>
#include <string>
#include <vector>

namespace phu::sequencing {

// Undo/redo stacks of commands (sec. 23.4). Distinct from juce::UndoManager (D-56). [MT]
class UndoManager {
public:
    explicit UndoManager(std::size_t maxDepth = constants::defaultUndoDepth);

    // Executes, pushes, clears the redo stack and drops the oldest entries beyond maxDepth. Null is ignored.
    void execute(std::unique_ptr<Command> command);
    // Executes in order; undone as one step in reverse order. Empty input is ignored.
    void executeMacro(std::vector<std::unique_ptr<Command>> commands);

    void undo();
    void redo();
    void clear() noexcept;

    [[nodiscard]] bool canUndo() const noexcept { return !undoStack_.empty(); }
    [[nodiscard]] bool canRedo() const noexcept { return !redoStack_.empty(); }
    [[nodiscard]] std::string getUndoName() const;  // empty if none
    [[nodiscard]] std::string getRedoName() const;

private:
    void push(std::unique_ptr<Command> executed);

    std::size_t maxDepth_;
    std::deque<std::unique_ptr<Command>> undoStack_;
    std::deque<std::unique_ptr<Command>> redoStack_;
};

}  // namespace phu::sequencing
