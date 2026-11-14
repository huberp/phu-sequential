# ARP Plugin Specification v2 - Sequencer Lanes

## 4. Sequencer Lanes

The plugin has exactly 8 sequencer lanes. Each lane is independently configurable. Each lane has a mute switch. Each lane has a lane color chosen with a color picker (Section 14). The lane color is purely visual and has no effect on MIDI processing. Each lane uses exactly one input mode.

### 4.1 Non-Exclusivity

Input sources are not exclusive. Multiple lanes may follow the same index or the same channel. Two lanes may follow the lowest chord note at the same time.

### 4.2 Chord-Follow Mode

The user sets a follow index in the range 0 to 15. The lane follows the chord note at this index. If the index is greater than or equal to the current chord size, the lane is silent. The lane plays again as soon as the chord contains enough notes. The lane is also silent while the chord is empty.

If a chord note is released while the chord still contains other notes, the sorting (Section 3.4) shifts: the follow index may then point to a different, still-held note. This is a pitch change, not a release – the lane switches to the new note, quantized to the next step start (Section 4.4). An immediate stop occurs only when the source becomes completely empty (chord empty or index outside the chord size).

### 4.3 Channel Mode

The user selects a MIDI channel from 1 to 16. The lane follows the most recently played note on this channel. The note is effective only while it is held. There is no note stack: when the followed note is released, the lane stops immediately if Latch is off (Sections 4.6, 9.2), even if older notes on this channel are still held. Before the first note on the channel, the lane is silent.

### 4.4 Adoption of New Input

A pitch change of the input (new note or re-sorting according to Section 4.2) is adopted only at the next step start of the respective lane. A running step plays its note until its scheduled end. A release of the source (the source no longer delivers a note) takes precedence over this quantization and stops the lane immediately according to Section 9.2.

### 4.5 Position During Silence

Lanes are always free-running. Silence means: the lane's source delivers no note (chord empty, index outside the chord size, or no note held on the channel). The step position continues during silence; on resumption the lane continues at the advanced position (Section 10). Chord changes while notes are held (legato transitions and re-sortings) never affect the position. Mute is not silence in this sense: the step position continues during mute. There is no reset-on-input mode.

### 4.6 Latch Mode

Each lane has a switchable latch parameter. Latch off: the lane stops according to Section 9.2 when the input note is released. Latch on: when the input note is released, the lane continues on the most recently followed note. With latch on, each newly played note of the source updates the followed note at the next step start. Latch on ends through transport stop, mute, bypass (Section 21), or switching latch off. When latch ends, all sounding notes of the lane immediately receive Note-Offs, and the latched note (latch memory) is cleared. Mute and transport stop clear the latch memory; the Latch parameter itself stays unchanged. After un-mute or the next transport start, the lane plays only if its source currently has a held note (the live held-note state, Section 10); the latch memory is not restored. The latch parameter is automatable.
