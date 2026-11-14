# lib/sequencing — Reusable core library

**Namespace**: `phu::sequencing`  
**Rule**: No JUCE headers; all data types are portable.

## Modules

| File(s) | ID | Purpose |
|---------|----|---------| 
| `Constants.h` | M01 | Central limits (MIDI, velocity, lanes, timing, colors) |
| `MusicalTime.h`, `NoteValues.h` | M02 | Units: `Beats`, `TickCount`, `NoteLength` enum |
| `StepTiming.h/.cpp` | M03 | Grid layout: uniform or text-grid layouts |
| `SampleConversion.h` | M04 | Tick ↔ sample conversion (tempo-aware) |
| `TextGridParser.h/.cpp` | M05 | Parses text-grid definitions |
| `MidiEvent.h` | M06 | MIDI event types: `noteOn`, `noteOff`, `controlChange` |
| `HeldNoteTracker.h/.cpp` | M07 | Per-channel held notes + sustain pedal (sorted chord) |
| `sources/*` | M08 | Input mode strategies: channel, chord-follow |
| `velocity/*` | M09 | Velocity mode strategies: by-step, relative, delta, incoming |
| `StepData.h/.cpp` | M10 | Per-lane per-step config (timing, transpose, gate, etc.) |
| `OutputEventBuffer.h/.cpp`, `VoiceRegistry.h/.cpp` | M06/M11 | MIDI buffer, voice allocation & scheduling |

## Layout

```
lib/sequencing/
├── *.h/.cpp        # Core modules (M01–M07, M10–M11)
├── sources/        # M08: input mode strategies
└── velocity/       # M09: velocity mode strategies
```

## Key rules

- All public types in `phu::sequencing` namespace
- No JUCE; portable time units (`Beats`, `TickCount`)
- Thread-safe via immutable snapshots, never locks

**See** [docs/architecture/03-sequencing-modules.md](../docs/architecture/03-sequencing-modules.md) (§3), [spec/README.md](../spec/README.md) (§3–4)

