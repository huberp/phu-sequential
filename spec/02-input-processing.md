# ARP Plugin Specification v2 - Input Processing

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
