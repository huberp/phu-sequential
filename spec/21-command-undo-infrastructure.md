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
    virtual void execute() = 0;
    virtual void undo() = 0;
    virtual std::string getName() const = 0;
};
```

Each command:
- Encapsulates a single logical operation
- Knows how to perform the operation (`execute()`)
- Knows how to reverse the operation (`undo()`)
- Provides a human-readable name for UI display

### 23.3 Lane Command Base Class

`LaneCommand` is a specialized `Command` for operations that modify a single lane:

```cpp
class LaneCommand : public Command {
protected:
    LaneState savedState;
    int laneIndex;
    
public:
    LaneCommand(int laneIndex, const LaneState& currentState);
    void undo() override; // Restores savedState to the lane
};
```

The `LaneCommand` base class automatically:
- Stores the lane's state before modification (`savedState`)
- Restores this state when `undo()` is called
- Handles the lane index for multi-lane operations

### 23.4 Undo Manager

The `UndoManager` class manages the undo/redo stack:

```cpp
class UndoManager {
public:
    void execute(std::unique_ptr<Command> command);
    void undo();
    void redo();
    bool canUndo() const;
    bool canRedo() const;
    std::string getUndoName() const;
    std::string getRedoName() const;
    void clear();
    
    // For state persistence
    void saveState();
    void restoreState();
};
```

The undo manager:
- Maintains a stack of executed commands (undo stack)
- Maintains a stack of undone commands (redo stack)
- Clears the redo stack when a new command is executed
- Provides methods to check if undo/redo is available
- Provides command names for UI display

### 23.5 Reusability

The command and undo infrastructure is designed to be reusable:

1. **Decoupled from plugin specifics**: Commands operate on data structures (`LaneState`), not directly on plugin internals
2. **Generic Command base class**: Can be extended for non-lane operations (e.g., global settings)
3. **Standalone UndoManager**: Can be used in any context that needs undo/redo
4. **Testable**: Commands can be unit tested independently

### 23.6 Integration with LaneState

The command infrastructure leverages `LaneState` (Section 22) for:

- **State capture**: Commands store `LaneState` mementos before modification
- **State restoration**: Commands restore `LaneState` mementos when undoing
- **Consistency**: Same data representation for commands and VST3 state

This ensures that undo operations produce identical results to loading saved state.

### 23.7 Memory Management

- Commands are stored as `std::unique_ptr<Command>` in the undo/redo stacks
- Each command stores only the minimum necessary state (typically one `LaneState`)
- Undo stack depth is configurable (default: 100 commands)
- Memory usage is bounded and predictable

### 23.8 Thread Safety

The command and undo infrastructure is designed for the UI thread:

- All command execution and undo/redo operations occur on the UI thread
- The audio thread continues to use the current state during modifications
- State changes are atomic from the audio thread's perspective
