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
| SetStepVelocityCommand | Sets velocity for a specific step (active velocity mode) | step index, velocity value |
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
| SetStepCountCommand | Sets step count | count (1-64) |
| SetTimingSourceCommand | Sets timing source | timing source |
| SetBaseTimingCommand | Sets base timing | base timing, subdivision, swing (0-100%) |
| SetMuteCommand / SetLatchCommand | Sets mute / latch | on/off |
| SetLaneColorCommand | Sets lane color | RGB |
| SetTextGridCommand | Sets text grid content | grid string |

### 24.3 Command Implementation Pattern

Each lane command derives from `LaneCommand` (Section 23.3), takes `(laneIndex, currentState, params...)`, and implements `doExecute(LaneState&)` and `getName()`. Parameters are validated and clamped to the ranges of Sections 4-8 and 12.

### 24.4 Command Execution

When a lane command is executed:

1. `LaneCommand::execute()` captures the lane into `savedState` and calls `doExecute()`
2. The command is pushed onto the undo stack and the redo stack is cleared

When undoing, `savedState` is restored and the command moves to the redo stack.

When redoing, `execute()` is called again and the command moves back to the undo stack.

### 24.5 State Capture

All commands store the complete previous state (`savedState`). Commands with random elements (e.g., `FillRandomCommand`) must be deterministic given their parameters: the seed is a command parameter, so redo reproduces the same result.

### 24.6 Command Chaining

Multiple commands can be chained together to form complex operations:

```cpp
std::vector<std::unique_ptr<Command>> commands;
commands.push_back(std::make_unique<FillRandomCommand>(laneIndex, currentState, 50, seed));
commands.push_back(std::make_unique<RotateStepsCommand>(laneIndex, currentState, 2));
undoManager.executeMacro(std::move(commands)); // single undo step
```

Commands in a macro run in order; undo reverts them in reverse order. Since each command captures its state in `execute()`, no pre-captured state is stale.

### 24.7 UI Integration

Lane commands are triggered from the editor (Section 14) by menus, keyboard shortcuts, toolbars, or direct manipulation (e.g., clicking a step). Every user edit of lane state, including parameter changes made in the editor, goes through a command.

### 24.8 Command Discovery

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
