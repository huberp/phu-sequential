# ARP Plugin Specification v2 - Note-Off and Voice Management

## 9. Note-Off and Voice Management

### 9.1 Voice Tracking

The plugin manages each output note internally as its own voice. A Note-Off affects only its associated voice. Voices are tracked per (output channel, pitch) across all lanes and passed-through notes. Since MIDI cannot distinguish identical pitches on one channel, the following retrigger policy applies: if a new note is triggered while a voice with the same output channel and pitch is already sounding (from any lane or from pass-through), the plugin first sends a Note-Off for the sounding voice and immediately afterwards the Note-On of the new note, and ownership of that (output channel, pitch) transfers to the new owner. The previous owner's later Note-Off is suppressed; the note is released when the newest owner ends. This prevents hanging and prematurely ended notes at the receiver. Outgoing Note-Offs use note-off velocity 0.

### 9.2 Behavior on Release of the Input Note

When a lane's input note is released (the source no longer delivers a note, Section 4.4), the lane stops immediately, provided latch is off (Section 4.6). The plugin immediately sends Note-Offs for all currently sounding notes of this lane. This applies to both input modes. In channel mode, the Note-Off of the followed note ends the lane immediately (Section 4.3, no note stack). In chord-follow mode, re-sorting the chord does not count as a release (Section 4.2). With latch on, the lane continues on the most recently followed note.

### 9.3 Guaranteed Note-Offs

Every note sent receives a Note-Off under all conditions. This applies in particular to grid change, step count change, Timing Source change, playhead discontinuity, transport stop, mute, latch end, and bypass.

### 9.4 Voice Limit

At most 32 simultaneously sounding voices are allowed per lane (counting the voices currently owned by that lane; ownership per Section 9.1). When the limit is exceeded, the lane's oldest sounding voice immediately receives a Note-Off before the new note is triggered.
