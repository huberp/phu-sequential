# ARP Plugin Specification v2 - Output Routing

## 8. Output Routing

Each lane has an output channel parameter from 1 to 16. The default is 1. The output channel is a plain parameter, stored in state and presets (Section 13), and is never changed automatically when the source changes. Each lane has a transpose parameter from -24 to +24 semitones. The output pitch is the pitch of the input note plus the transpose value. If the transposed pitch is outside the MIDI range 0 to 127, the note is not sent (no clamping). Each output note internally receives a unique voice identifier (note tracking, Section 9.1).
