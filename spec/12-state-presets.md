# ARP Plugin Specification v2 - State and Presets

## 13. State and Presets

The plugin stores its complete state in the VST3 state: all global parameters, all lane parameters, the text grid content of all lanes, the lane colors, and all step data of all lanes. The plugin loads the state when a project is opened. The plugin ships with built-in factory presets. The plugin loads and saves additional preset files from a user folder; the file format is JUCE ValueTree XML. Presets cover the same scope as the plugin state, including the global parameters (Thru, Consume, Chord Input Channel). State and presets carry an integer format version. Loaders accept older versions via migration, ignore unknown fields, clamp invalid values, and reject newer versions by falling back to a safe default state.
