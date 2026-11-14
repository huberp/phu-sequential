# phu-sequential — Architecture — 02-units-and-types.md

## 2. Units, numeric types, core data types

### 2.1 Units and representation — D-3

| Quantity | Type | Unit | Notes |
|---|---|---|---|
| Musical position / duration (playhead, phase offset, note ends, swing advance) | `Beats` = `double` | quarter-note beats (= host PPQ) | §10, §17: core never stores samples or wall-clock |
| Grid structure (step lengths, step starts in loop, loop length) | `TickCount` = `std::int64_t` | ticks, `ticksPerBeat = 960` | exact: every allowed note length is an integer tick count (1/64 triplet = 40, 1/64 dotted = 90, 1/1 dotted = 5760). No drift when stepping loops. |
| Sample offset inside a block | `SampleOffset` = `std::int32_t` | samples, `[0, numSamples-1]` | computed only at the output boundary (§16.2, §5.4 of this doc) |
| Tempo | `double` | beats per minute | from host, constant within a block |
| Sample rate | `double` | Hz | |
| MIDI channel | `int` in APIs, `std::uint8_t` in events | **1..16** everywhere in the core (never 0-based) | |
| Pitch, velocity, controller | `int` in APIs, `std::uint8_t` in events | 0..127 | |
| Percentages (gate, swing, relative velocity) | `int` | integer percent | D-7 |
| Step index | `int` | 0-based internally; "step 1" in spec = index 0 | |
| Lane index | `int` | 0-based internally (0..7); user-visible / parameter IDs 1-based | |

Conversion of ticks to beats: `beats = ticks / 960.0`. Absolute beat of a step start is always computed as `phaseOffset + (loopIndex * loopLengthTicks + stepStartTicks[k]) / 960.0` (never accumulated), so floating error does not grow over time.

`beatEpsilon = 1.0e-6` beats (≈0.02 samples at 120 BPM/48 kHz) is the only tolerance used for "is exactly at a step start" decisions.

### 2.2 Core data type overview

| Concept | Type | Module |
|---|---|---|
| Event (note on/off, CC64 — the only events the core receives, D-31) | `sequencing::MidiEvent` | M06 |
| Output event list | `sequencing::OutputEventBuffer` | M06 |
| Note length (1/d + subdivision) | `sequencing::NoteLength` | M02 |
| Lane grid | `sequencing::GridLayout` | M03 |
| Parsed text grid | `sequencing::TextGrid` | M05 |
| Step data (on/off + value per velocity mode) | `sequencing::StepData`, `sequencing::StepPattern` | M10 |
| Lane configuration (host parameters of one lane) | `arpeggiator::LaneParameters` | M12 |
| Lane non-parameter data seen by audio | `arpeggiator::LanePattern` | M12 |
| Global parameters | `arpeggiator::GlobalParameters` | M12 |
| Full requested configuration | `arpeggiator::SequencerConfiguration` | M17 |
| Block input (events, transport, bypass) | `arpeggiator::BlockInput` (M17), `arpeggiator::TransportInfo` (M15) | M17/M15 |
| Block output | `sequencing::OutputEventBuffer` (filled by `SequencerCore::processBlock`) | M06/M17 |
| Beat ↔ sample mapping of one block | `arpeggiator::BlockClock` | M13 |

---
