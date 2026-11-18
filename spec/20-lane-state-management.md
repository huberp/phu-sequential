# ARP Plugin Specification v2 - Lane State Management

## 22. Lane State Management

### 22.1 LaneState as Canonical Representation

Each lane's complete configuration and step data is represented by a single `LaneState` object. This object serves as the sole source of truth for all lane-related data across the plugin, including: step on/off states and velocity values (per step and velocity mode), input mode, follow index (0 to 15) or follow channel (1 to 16), output channel, transpose, gate, timing source, base timing, subdivision, swing, step count, text grid content, velocity mode, latch, mute, and lane color (RGB).

Automatable lane parameters (Section 12) remain host parameters. `LaneState` holds their values as a snapshot; applying a `LaneState` writes them through the parameter system with host notification.

### 22.2 Serialization

`LaneState` is a plain C++ value type in the reusable core (no JUCE, Section 15). Conversion to and from JUCE `ValueTree` (`toValueTree()` / `fromValueTree(const ValueTree&)`) lives in the JUCE adapter and uses the ValueTree XML format of Section 13.

### 22.3 Copy Semantics

`LaneState` has value semantics. Copy construction and assignment produce a deep copy; `operator==` compares all lane properties.

### 22.4 Integration with VST3 State

The plugin's complete state (Section 13) includes an array of 8 `LaneState` objects, one per lane. When saving plugin state, each lane's `LaneState` is serialized as one child of the plugin state `ValueTree`. When loading, each child is deserialized and assigned to its lane. Presets (Section 13) use the same lane serialization.

### 22.5 Versioning

`LaneState` serialization includes a version number. Loading follows Section 13: older versions are migrated, unknown fields are ignored, invalid values are clamped, and newer versions fall back to the default state.

