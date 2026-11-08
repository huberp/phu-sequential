# ARP Plugin Specification v2

## 1. Overview

The plugin is an arpeggiator sequencer implemented as a VST3 MIDI effect. The plugin receives MIDI notes as input, sorts them, and feeds them into exactly 8 independent sequencer lanes. Each lane generates its own sequence of output notes from its input note. The plugin has no audio path. The plugin processes MIDI events exclusively.

## 2. Plugin Format and Category

The target format is VST3. The plugin is implemented as a MIDI effect (event input, event output, no audio path). The plugin uses the VST3 category for MIDI effects. The plugin supports state saving according to Section 13.

## 3. Input Processing

### 3.1 Chord Definition

The currently played chord consists exclusively of held notes. A note enters the chord on Note-On. A note leaves the chord on Note-Off. A Note-On with velocity 0 is treated as a Note-Off (MIDI convention). The chord is therefore at all times the set of all currently held notes. The chord contains each pitch at most once. A Note-On of an already held pitch replaces the existing note in the chord. The sustain pedal (CC64) applies per channel. While the pedal of a channel is on (value of at least 64), the notes of that channel continue to count as held after their Note-Off until the pedal is turned off (value below 64). When the pedal is turned off, all notes that are no longer held by a key leave the chord. The pedal logic applies equally to the chord input channel and to the channels of the channel-mode lanes.

### 3.2 Input Channel Filter

Chord-follow mode (Section 4.2) listens to exactly one globally configurable chord input channel (1 to 16). Only notes on this channel form the chord. The chord input channel is an automatable global parameter (Section 12). Channel mode (Section 4.3) listens independently to channels 1 to 16 according to each lane's setting.

### 3.3 Pass-Through

The plugin can pass input notes through to the output. Pass-through is switched on and off by the global parameter Thru and applies to all 16 MIDI channels. When pass-through is off, the plugin outputs only the generated sequencer notes.

The plugin additionally has the global parameter Consume. When Consume is on, input notes on all channels followed by at least one lane (the chord input channel and all follow channels of the channel-mode lanes) are not passed through, regardless of the Thru parameter. Consume prevents double triggering when a lane's output channel matches the followed channel. Pass-through is tracked per note: a Note-Off is passed through if and only if its Note-On was passed through, regardless of later changes to Thru, Consume, or follow channels. This guarantees that no passed-through note hangs. The sustain pedal does not alter this rule: a passed-through note is released by its key's Note-Off, while the downstream sustain behavior is governed by the passed-through CC64.

Non-note events (CC, pitch bend, aftertouch, program change) are always passed through unchanged, regardless of Thru and Consume. CC64 is additionally interpreted for the hold logic (Section 3.1). CC120 (All Sound Off) and CC123 (All Notes Off) are merely passed through and trigger no special internal handling; voice management follows Section 9 exclusively. Thru and Consume are automatable.

### 3.4 Sorting

For index usage, notes are sorted ascending by pitch. Index 0 denotes the lowest note. Index 1 denotes the next higher note.

## 4. Sequencer Lanes

The plugin has exactly 8 sequencer lanes. Each lane is independently configurable. Each lane has a mute switch. Each lane uses exactly one input mode.

### 4.1 Non-Exclusivity

Input sources are not exclusive. Multiple lanes may follow the same index or the same channel. Two lanes may follow the lowest chord note at the same time.

### 4.2 Chord-Follow Mode

The user sets a follow index in the range 0 to 15. The lane follows the chord note at this index. If the index is greater than or equal to the current chord size, the lane is silent. The lane plays again as soon as the chord contains enough notes. The lane is also silent while the chord is empty.

If a chord note is released while the chord still contains other notes, the sorting (Section 3.4) shifts: the follow index may then point to a different, still-held note. This is a pitch change, not a release – the lane switches to the new note, quantized to the next step start (Section 4.4). An immediate stop occurs only when the source becomes completely empty (chord empty or index outside the chord size).

### 4.3 Channel Mode

The user selects a MIDI channel from 1 to 16. The lane follows the most recently played note on this channel. The note is effective only while it is held. There is no note stack: when the followed note is released, the lane stops immediately (Section 9.2), even if older notes on this channel are still held. Before the first note on the channel, the lane is silent.

### 4.4 Adoption of New Input

A pitch change of the input (new note or re-sorting according to Section 4.2) is adopted only at the next step start of the respective lane. A running step plays its note until its scheduled end. A release of the source (the source no longer delivers a note) takes precedence over this quantization and stops the lane immediately according to Section 9.2.

### 4.5 Position During Silence

Lanes are always free-running. Silence means: the lane's source delivers no note (chord empty, index outside the chord size, or no note held on the channel). The step position continues during silence; on resumption the lane continues at the advanced position (Section 10). Chord changes while notes are held (legato transitions and re-sortings) never affect the position. Mute is not silence in this sense: the step position continues during mute. There is no reset-on-input mode.

### 4.6 Latch Mode

Each lane has a switchable latch parameter. Latch off: the lane stops according to Section 9.2 when the input note is released. Latch on: when the input note is released, the lane continues on the most recently followed note. With latch on, each newly played note of the source updates the followed note at the next step start. Latch on ends through transport stop, mute, bypass (Section 21), or switching latch off. When latch ends, all sounding notes of the lane immediately receive Note-Offs, and the latched note (latch memory) is cleared. Mute and transport stop clear the latch memory; the Latch parameter itself stays unchanged. After un-mute or the next transport start, the lane plays only if its source currently has a held note (the live held-note state, Section 10); the latch memory is not restored. The latch parameter is automatable.

## 5. Timing

### 5.1 Base Timing

A lane uses a base timing from a list. The list contains 1/32, 1/16, 1/8, 1/4, 1/2, and 1/1. In addition, the user selects a subdivision: straight, triplet, or dotted. A swing amount of 0 to 100 percent is adjustable. Swing shifts only the start position of steps with an even 1-based index (the 2nd, 4th, 6th, … step; odd when counted 0-based) backward; step 1 is never shifted. Counting restarts at step 1 for each loop. The shift is at most half the step length: 100 percent swing corresponds to a shift of 50 percent of the step length. Note lengths remain unchanged; the grid start positions of all other steps remain unchanged. As a result, a shifted note may extend into the following step; this overlap is allowed (voice management according to Section 9). Swing applies only to base timing and not to the text grid. The base timing and the subdivision determine the grid and the length of each step.

### 5.2 Custom Grid (Text Grid)

Alternatively, the user can enter a grid as text. The format is a comma-separated list of n/d fractions. An optional suffix t denotes triplet, an optional suffix . denotes dotted; the two suffixes are mutually exclusive. Example: "1/8t, 1/8., 1/4" defines 3 steps. Only musical values are allowed: the numerator is exactly 1, and the denominator is one of 1, 2, 4, 8, 16, 32, 64; each entry may carry an optional suffix t (triplet) or . (dotted). Other fractions (for example 3/8) are invalid. The total loop length is not restricted. A text grid has at most 64 entries. Each entry defines the length and grid position of one step. The loop length of the lane is the sum of all step lengths. When a new text grid is applied, the step data (on/off and velocity values) are transferred position by position to the new steps; surplus data is discarded, and missing steps receive defaults (on, default velocity of the active mode).

### 5.3 Text Grid Validation

Invalid entries are marked red in the UI. A grid with invalid entries is not applied. The lane keeps its last valid grid.

### 5.4 Step Count

A lane with base timing has a configurable step count. The step count is between 1 and 64. The step count is freely selectable per lane. Example: one lane with 16 steps, another lane with 7 steps. The loop length is the step count times the step length. When the step count is increased, the new steps receive the state on and default velocity values. When it is decreased, the data of the removed steps is discarded; a later increase does not restore old data. When a step count or grid change takes effect at the next step start, the lane's phase offset (Section 10) is re-anchored so that the current step index is kept modulo the new step count, and the new loop starts at that step start. No step is skipped or repeated by the re-anchoring itself.

## 6. Steps

Each step has an on/off switch. An off step produces no note and is silent for its duration. An on step plays a note. Each lane has a gate parameter in percent. The note length is the step length times the gate value, divided by 100. The gate value is between 1 and 200. The default is 100 percent. With gate above 100 percent, notes overlap; overlap is allowed. A lane is polyphonic. The gate parameter is configurable per lane and automatable.

## 7. Velocity

Each lane has exactly one velocity mode. The default mode is "incoming velocity". Each step stores its own value per velocity mode; switching the mode restores the step values last set in that mode. No mode switch overwrites the values of another mode.

### 7.1 Modes

Incoming velocity: the note receives the velocity of the input note at the step trigger time; the step values of this mode remain unused. By step: each step has an absolute velocity value from 0 to 127. Delta by step: each step has a delta value from -127 to +127. The delta value is added to the velocity of the input note. Relative: each step has a percentage value from 0 to 200. The note receives the input velocity times the percentage value, divided by 100.

### 7.2 Clamping

All calculated velocity values are clamped to 0 to 127. A note with a calculated velocity of 0 is not sent.

### 7.3 Sampling

The input velocity is sampled at the step trigger; with latch (Section 4.6), the velocity of the most recently followed note applies. Velocity changes during a sounding step do not affect the note that has already been triggered.

## 8. Output Routing

Each lane has an output channel parameter from 1 to 16. The default is 1. The output channel is a plain parameter, stored in state and presets (Section 13), and is never changed automatically when the source changes. Each lane has a transpose parameter from -24 to +24 semitones. The output pitch is the pitch of the input note plus the transpose value. If the transposed pitch is outside the MIDI range 0 to 127, the note is not sent (no clamping). Each output note internally receives a unique voice identifier (note tracking, Section 9.1).

## 9. Note-Off and Voice Management

### 9.1 Voice Tracking

The plugin manages each output note internally as its own voice. A Note-Off affects only its associated voice. Voices are tracked per (output channel, pitch) across all lanes and passed-through notes. Since MIDI cannot distinguish identical pitches on one channel, the following retrigger policy applies: if a new note is triggered while a voice with the same output channel and pitch is already sounding (from any lane or from pass-through), the plugin first sends a Note-Off for the sounding voice and immediately afterwards the Note-On of the new note, and ownership of that (output channel, pitch) transfers to the new owner. The previous owner's later Note-Off is suppressed; the note is released when the newest owner ends. This prevents hanging and prematurely ended notes at the receiver. Outgoing Note-Offs use note-off velocity 0.

### 9.2 Behavior on Release of the Input Note

When a lane's input note is released (the source no longer delivers a note, Section 4.4), the lane stops immediately, provided latch is off (Section 4.6). The plugin immediately sends Note-Offs for all currently sounding notes of this lane. This applies to both input modes. In channel mode, the Note-Off of the followed note ends the lane immediately (Section 4.3, no note stack). In chord-follow mode, re-sorting the chord does not count as a release (Section 4.2). With latch on, the lane continues on the most recently followed note.

### 9.3 Guaranteed Note-Offs

Every note sent receives a Note-Off under all conditions. This applies in particular to grid change, step count change, transport stop, mute, latch end, and bypass.

### 9.4 Voice Limit

At most 32 simultaneously sounding voices are allowed per lane (counting the voices currently owned by that lane; ownership per Section 9.1). When the limit is exceeded, the lane's oldest sounding voice immediately receives a Note-Off before the new note is triggered.

## 10. Transport and Synchronization

The sequencer runs host-synchronized. The sequencer follows start and stop of the host transport. On transport start, all lanes reset to step 1. On transport stop, all lanes stop immediately, and all sounding notes receive Note-Offs. Without a running host transport, all lanes are silent; the plugin has no internal clock as a fallback. The sources (chord and channel notes) always reflect the live held-note state: keys held at transport start drive the lanes immediately. Only latch memory is cleared at transport stop (Section 4.6).

The step position of a lane is defined as (playhead beats − lane phase offset) mod loop length, in beats (musical time, PPQ), not in samples. The phase offset is set by transport start (the playhead maps to step 1) and by step count or grid changes (Section 5.4). Since positions and note lengths are defined in beats, tempo changes take effect automatically and immediately on step positions and on the Note-Off times of notes already sounding. Any playhead discontinuity while the transport keeps running (a backward move, or a forward jump beyond one buffer, for example a host loop wrap or locate) sends Note-Offs for all sounding notes, applies all pending changes (Sections 4.4, 11), and re-derives the lane position from the playhead. Only transport start sets the phase offset to map the playhead to step 1. Events are sent with sample-accurate offsets within the buffer.

## 11. Live Changes

All changes while the sequencer is running are glitch-free. Changes to step count or grid take effect at the next step start of the respective lane (position wrapping according to Section 5.4). Changes to step on/off and velocity values take effect immediately. Changes to the input mode take effect at the next step start. Changes to transpose, gate, swing, and output channel affect newly triggered notes; notes already sounding keep their pitch, their velocity, and their scheduled Note-Off time, so that the Note-Off guarantee (Section 9.3) is upheld.

## 12. Parameters and Automation

Only finite, named parameters are automatable.

Global parameters: Thru, Consume, Chord Input Channel.

Lane parameters (per lane): Mute, Latch, Input Mode, Follow Index (0 to 15), Follow Channel, Output Channel, Transpose, Gate, Base Timing, Subdivision, Swing Amount, Step Count, Velocity Mode.

Step data (on/off, velocity values per step and mode) and the text grid are non-automatable plugin state (Section 13). The text grid is not a host parameter.

## 13. State and Presets

The plugin stores its complete state in the VST3 state: all global parameters, all lane parameters, and all step data of all lanes. The plugin loads the state when a project is opened. The plugin ships with built-in factory presets. The plugin loads and saves additional preset files from a user folder; the file format is JUCE ValueTree XML. Presets cover the same scope as the plugin state, including the global parameters (Thru, Consume, Chord Input Channel). State and presets carry an integer format version. Loaders accept older versions via migration, ignore unknown fields, clamp invalid values, and reject newer versions by falling back to a safe default state.

## 14. UI

Each lane has a step display. The step display shows the width proportions of the selected timing. Each step can be toggled on/off by clicking. Each step has a vertical drag bar for editing its velocity value; the drag bar edits and displays the value of the currently active velocity mode (in incoming velocity mode the drag bar is disabled). With many steps (e.g. 16), the lane display remains horizontally scrollable or switchable. Each lane shows a playback position indicator. The text grid is presented as editable per lane and is validated (Section 5.3).

## 15. Implementation and Tooling

The language is C++20. The framework is JUCE. The target is VST3 on Windows, macOS, and Linux. Direct Steinberg SDK calls are permitted only where JUCE provides no coverage. The editor itself is implemented natively as a custom JUCE editor.

## 16. Architecture

### 16.1 Layers

The code strictly separates reusable code from plugin-specific code. Reusable modules are intended for use in other plugins and are therefore pure C++20 without JUCE includes (a prerequisite for host-free and JUCE-free testability according to Section 18). At minimum, the following are reusable: timing math, grid parser, velocity calculation, index determination, chord management, voice management. Plugin-specific code wires the reusable modules together exclusively via composition. The JUCE integration (AudioProcessor, parameter binding, MIDI I/O, editor) forms its own adapter layer; only this layer contains JUCE dependencies.

### 16.2 Timing Math

The timing values 1/32 to 1/1 are defined declaratively as data in exactly one place. Straight, triplet, and dotted are modifiers on a straight base timing. The modifier factors (triplet = 2/3, dotted = 3/2) are defined as named constants in exactly one place. The conversion of a note length (in beats) to samples via BPM and sample rate is its own pure computation unit at the boundary to the output. The conversion is free of side effects and free of host dependencies.

### 16.3 Magic Constants

The code contains no magic constants. All numeric values (timing list, modifier factors, velocity limits, channel count, lane count, step limits, follow index maximum, voice limit) are named constants in a central place. The velocity limits (0, 127, delta range) are defined in exactly one place.

### 16.4 Selection Logic Instead of Branching Orgies

Velocity modes and index determination are implemented data-driven or as strategy objects. Each option is its own named unit. The available options are registered and visible in exactly one place. New modes require a new unit plus registration, with no change to existing branches. Long if/else or switch chains over modes are not permitted.

### 16.5 Separation of Concerns

Each module has exactly one responsibility. Input processing, chord management, lane engine, timing, output generation, and UI are separate units. No module knows the internals of another. Communication takes place via narrow, well-defined interfaces.

## 17. Robustness and Real-Time

The audio thread performs no allocations, locks, or blocking calls. Parameter changes travel from the UI thread to the audio thread via atomic values or lock-free queues. The core stores time exclusively in beats (PPQ), never in samples and never as wall-clock time; conversion to samples happens exclusively at the output boundary for buffer offsets (Sections 10, 16.2). The core validates all input values; invalid values are rejected or clamped. RAII applies everywhere; raw new/delete are not permitted. The core throws no exceptions in the audio path.

## 18. Testability

Unit tests are mandatory. The test framework is Catch2. The reusable modules are testable without a host and without JUCE (Section 16.1). Timing math, grid parser, velocity calculation, index determination, and voice management have their own tests. Edge cases of the specification (index greater than chord, chord re-sorting, velocity clamping, overlap, retrigger policy, Transport reset, playhead discontinuity handling, cross-lane voice retrigger and ownership transfer, per-note pass-through tracking, Note-Off guarantee, voice limit, state version migration) are test cases.

## 19. Code Style

The code is idiomatic, performant, modular, robust, maintainable, and extensible. One unit of responsibility per file. Reusable modules reside in their own directory and namespace. Plugin-specific code resides in a separate directory and namespace. Names are self-descriptive and free of abbreviations. Includes are minimal; forward declarations are used where possible. Comments justify decisions and do not describe obvious syntax.

## 20. Initial State

Each lane starts with base timing 1/16, subdivision straight, swing 0, step count 16, and all steps on. The velocity mode is incoming velocity. The input mode is chord-follow with index 0. The chord input channel is 1. The output channel is 1 (Section 8). Transpose is 0. Gate is 100 percent. Latch is off. Mute is off. Thru is off. Consume is on.

## 21. Bypass Behavior

On host bypass, the plugin passes all input events through to the output unchanged. All lanes stop immediately; latch ends (Section 4.6). All sounding notes of the lanes immediately receive Note-Offs. When bypass ends, each lane takes over the position derived from the host playhead (Section 10). No reset to step 1 takes place.
