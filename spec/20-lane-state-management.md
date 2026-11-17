# ARP Plugin Specification v2 - Lane State Management

## 22. Lane State Management

### 22.1 LaneState as Canonical Representation

Each lane's complete configuration and step data is represented by a single `LaneState` object. This object serves as the sole source of truth for all lane-related data across the plugin, including:

- Step on/off states and velocity values (per step, per velocity mode)
- Input mode (chord-follow or channel mode)
- Follow index (0-15) or follow channel (1-16)
- Output channel (1-16)
- Transpose value (-24 to +24 semitones)
- Gate percentage (1-200)
- Timing source (base timing or text grid)
- Base timing parameters (step length, subdivision, swing)
- Text grid content
- Velocity mode
- Latch state
- Mute state
- Lane color (RGB)

### 22.2 Serialization

The `LaneState` class provides methods for serialization to and from JUCE `ValueTree` format:

- `LaneState::toValueTree()`: Converts lane state to a `ValueTree` for VST3 state persistence
- `LaneState::fromValueTree(const ValueTree&)`: Restores lane state from a `ValueTree`

Serialization format uses JUCE ValueTree XML, matching the plugin's existing state format (Section 13).

### 22.3 Copy Semantics

`LaneState` supports value semantics with:

- Copy constructor: Creates a deep copy of all lane data
- Copy assignment operator: Replaces current state with a deep copy
- Equality operator: Compares all lane properties for equality

The `clone()` method returns a new `LaneState` instance that is a bitwise copy of the current state.

### 22.4 Integration with VST3 State

The plugin's complete state (Section 13) includes an array of 8 `LaneState` objects, one per lane. When saving plugin state:

1. Each lane's `LaneState` is serialized to a `ValueTree`
2. All 8 lane `ValueTree` objects are added as children to the plugin state `ValueTree`
3. The plugin state is serialized to XML and stored in the VST3 state

When loading plugin state:

1. The plugin state XML is parsed to a `ValueTree`
2. Each child `ValueTree` (representing a lane) is deserialized to a `LaneState`
3. The 8 `LaneState` objects are assigned to their respective lanes

### 22.5 Versioning

`LaneState` serialization includes a version number. When loading state:

- Older versions are migrated to the current format
- Unknown fields are ignored
- Invalid values are clamped to valid ranges
- Newer versions cause fallback to default state

This matches the versioning behavior described in Section 13.
