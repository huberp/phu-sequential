# ARP Plugin Specification v2 - Lane Commands

## 24. Lane Commands

### 24.1 Overview

Lane commands are specific `LaneCommand` implementations (Section 23) that perform operations on individual lanes. Each command modifies lane state and supports undo/redo through the command infrastructure.

### 24.2 Command Categories

Lane commands are organized into the following categories:

#### 24.2.1 Step Manipulation Commands

| Command | Description | Parameters |
|---------|-------------|------------|
| ReverseStepsCommand | Reverses the order of all steps in the lane | None |
| RotateStepsCommand | Rotates steps left or right by N positions | rotation amount (int) |
| InvertStepsCommand | Flips the on/off state of all steps | None |

#### 24.2.2 Pattern Generation Commands

| Command | Description | Parameters |
|---------|-------------|------------|
| FillRandomCommand | Fills lane with random step states | probability (0-100%), seed (int) |
| ArithmeticSequenceCommand | Generates step values using arithmetic progression | start value, common difference, step count |
| EuclideanPatternCommand | Generates steps using Euclidean algorithm | hits, steps, rotation |

#### 24.2.3 Step Editing Commands

| Command | Description | Parameters |
|---------|-------------|------------|
| SetStepOnCommand | Sets a specific step to on/off | step index, on/off state |
| SetStepVelocityCommand | Sets velocity for a specific step | step index, velocity value |
| ClearLaneCommand | Turns all steps off | None |
| FillLaneCommand | Turns all steps on | None |

#### 24.2.4 Configuration Commands

| Command | Description | Parameters |
|---------|-------------|------------|
| SetInputModeCommand | Sets lane input mode | input mode (chord-follow/channel) |
| SetFollowIndexCommand | Sets follow index | index (0-15) |
| SetFollowChannelCommand | Sets follow channel | channel (1-16) |
| SetOutputChannelCommand | Sets output channel | channel (1-16) |
| SetTransposeCommand | Sets transpose value | semitones (-24 to +24) |
| SetGateCommand | Sets gate percentage | gate (1-200%) |
| SetVelocityModeCommand | Sets velocity mode | velocity mode |
| SetTimingSourceCommand | Sets timing source | timing source |
| SetBaseTimingCommand | Sets base timing | timing value, subdivision, swing |
| SetTextGridCommand | Sets text grid content | grid string |

### 24.3 Command Implementation Pattern

All lane commands follow the same pattern:

```cpp
class ConcreteCommand : public LaneCommand {
public:
    ConcreteCommand(int laneIndex, const LaneState& currentState, 
                    ParamType param1, ParamType param2);
    
    void doExecute(LaneState& lane) override {
        // Modify lane state based on parameters
    }
    
    std::string getName() const override {
        return "Command Name";
    }
};
```

### 24.4 Command Execution

When a lane command is executed:

1. The current lane state is captured into `savedState` (by `LaneCommand` base class)
2. `doExecute()` is called to modify the lane
3. The command is pushed onto the undo stack
4. The redo stack is cleared

When undoing:

1. The command's `savedState` is restored to the lane
2. The command is moved from undo stack to redo stack

When redoing:

1. The command's `doExecute()` is called again
2. The command is moved from redo stack to undo stack

### 24.5 Non-Deterministic Commands

Commands that produce different results each time (e.g., `FillRandomCommand`) must store the complete previous state in `savedState`. The parameters (seed, probability) are stored separately to allow redo.

### 24.6 Deterministic Commands

Commands that produce the same result when executed multiple times with the same parameters (e.g., `ReverseStepsCommand`, `SetStepOnCommand`) can optionally:

- Store only the parameters and recompute on redo (more memory efficient)
- Store the complete previous state (simpler, more consistent)

For consistency and simplicity, **all commands store the complete previous state**.

### 24.7 Command Chaining

Multiple commands can be chained together to form complex operations:

```cpp
// Example: Create a "randomize and rotate" macro
std::vector<std::unique_ptr<LaneCommand>> commands;
commands.push_back(std::make_unique<FillRandomCommand>(laneIndex, currentState, 50, seed));
commands.push_back(std::make_unique<RotateStepsCommand>(laneIndex, currentState, 2));

// Execute as a single undoable action
undoManager.executeMacro(std::move(commands));
```

The macro command stores all individual commands and can undo/redo them as a group.

### 24.8 UI Integration

Lane commands are triggered by:

- **Menu items**: Each command has a corresponding menu action
- **Keyboard shortcuts**: Commands can be assigned keyboard shortcuts
- **Toolbars**: Common commands can be placed on toolbars
- **Direct manipulation**: Some commands are triggered by direct UI interaction (e.g., clicking a step to toggle it)

### 24.9 Command Discovery

The plugin provides a registry of available lane commands:

```cpp
class LaneCommandRegistry {
public:
    static LaneCommandRegistry& getInstance();
    
    void registerCommand(const std::string& name, 
                        std::function<std::unique_ptr<LaneCommand>(int, const LaneState&)> factory);
    
    std::unique_ptr<LaneCommand> createCommand(const std::string& name, 
                                                 int laneIndex, 
                                                 const LaneState& state) const;
    
    std::vector<std::string> getCommandNames() const;
};
```

This allows dynamic discovery of available commands for UI generation.
