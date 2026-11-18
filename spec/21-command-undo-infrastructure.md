# ARP Plugin Specification v2 - Command and Undo Infrastructure

## 23. Command and Undo Infrastructure

### 23.1 Overview

The plugin implements a command-based undo/redo system that allows users to reverse any lane modification. The system is designed to be reusable across different parts of the plugin and potentially in other plugins.

### 23.2 Command Pattern

All state-modifying operations are encapsulated as `Command` objects:

```cpp
class Command {
public:
    virtual ~Command() = default;
    virtual void execute() = 0; // also used for redo
    virtual void undo() = 0;
    virtual std::string getName() const = 0;
};
```

Each command encapsulates a single logical operation, can perform (`execute()`) and reverse (`undo()`) it, and provides a human-readable name for the UI.

### 23.3 Lane Command Base Class

`LaneCommand` is a specialized `Command` for operations that modify a single lane:

```cpp
class LaneCommand : public Command {
protected:
    LaneState savedState;
    int laneIndex;
    virtual void doExecute(LaneState& lane) = 0;

public:
    LaneCommand(int laneIndex, const LaneState& currentState);
    void execute() override; // captures savedState, then doExecute()
    void undo() override;    // restores savedState to the lane
};
```

The base class stores the lane state before each execution (`savedState`) and restores it on `undo()`.

### 23.4 Undo Manager

The `UndoManager` class manages the undo/redo stack:

```cpp
class UndoManager {
public:
    void execute(std::unique_ptr<Command> command);
    void executeMacro(std::vector<std::unique_ptr<Command>> commands); // one undo step
    void undo();
    void redo();
    bool canUndo() const;
    bool canRedo() const;
    std::string getUndoName() const;
    std::string getRedoName() const;
    void clear();
};
```

The undo manager keeps an undo stack of executed commands and a redo stack of undone commands; executing a new command clears the redo stack.

### 23.5 Reusability

`Command` and `UndoManager` are pure C++20 without JUCE (Section 15), operate on data structures (`LaneState`) rather than plugin internals, can be extended for non-lane operations (e.g., global settings), and are unit-testable independently (Section 18). Undo history is not part of the VST3 state.

### 23.6 Integration with LaneState

Commands store `LaneState` (Section 22) mementos before modification and restore them on undo, so undo produces the same result as loading saved state.

### 23.7 Memory Management

Commands are stored as `std::unique_ptr<Command>`; each stores one `LaneState` at most. The undo stack depth is bounded and configurable (default: 100 commands).

### 23.8 Thread Safety

All command execution and undo/redo occur on the UI thread. Resulting lane changes reach the audio thread only via the mechanisms of Section 17 (atomics, lock-free queues) and take effect as glitch-free live changes (Section 11); the audio thread never touches the command stacks.
