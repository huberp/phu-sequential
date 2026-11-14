# ARP Plugin Specification v2 - Implementation and Architecture

## 15. Implementation and Tooling

The language is C++20. The framework is JUCE. The target is VST3 on Windows, macOS, and Linux. Direct Steinberg SDK calls are permitted only where JUCE provides no coverage. The editor itself is implemented natively as a custom JUCE editor.

## 16. Architecture

### 16.1 Layers

The code strictly separates reusable code from plugin-specific code. Reusable modules are intended for use in other plugins and are therefore pure C++20 without JUCE includes (a prerequisite for host-free and JUCE-free testability according to Section 18). At minimum, the following are reusable: timing math, grid parser, velocity calculation, index determination, chord management, voice management. Plugin-specific code wires the reusable modules together exclusively via composition. The JUCE integration (AudioProcessor, parameter binding, MIDI I/O, editor) forms its own adapter layer; only this layer contains JUCE dependencies.

### 16.2 Timing Math

The timing values 1/32 to 1/1 are defined declaratively as data in exactly one place. Straight, triplet, and dotted are modifiers on a straight base timing. The modifier factors (triplet = 2/3, dotted = 3/2) are defined as named constants in exactly one place. The conversion of a note length (in beats) to samples via BPM and sample rate is its own pure computation unit at the boundary to the output. The conversion is free of side effects and free of host dependencies.

### 16.3 Magic Constants

The code contains no magic constants. All numeric values (timing list, modifier factors, velocity limits, channel count, lane count, step limits, follow index maximum, voice limit, default lane color palette, minimum step display width) are named constants in a central place. The velocity limits (0, 127, delta range) are defined in exactly one place.

### 16.4 Selection Logic Instead of Branching Orgies

Velocity modes and index determination are implemented data-driven or as strategy objects. Each option is its own named unit. The available options are registered and visible in exactly one place. New modes require a new unit plus registration, with no change to existing branches. Long if/else or switch chains over modes are not permitted.

### 16.5 Separation of Concerns

Each module has exactly one responsibility. Input processing, chord management, lane engine, timing, output generation, and UI are separate units. No module knows the internals of another. Communication takes place via narrow, well-defined interfaces.
