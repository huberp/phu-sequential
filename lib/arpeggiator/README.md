# lib/arpeggiator — Plugin-specific core library

**Namespace**: `phu::arpeggiator`  
**Rule**: No JUCE headers; portable audio/sequencing I/O. Depends on `lib/sequencing`.

## Modules

| File(s) | ID | Purpose |
|---------|----|---------| 
| `LaneConfiguration.h/.cpp` | M12 | Per-lane config (timing source, gate %, transpose, input mode) |
| `BlockClock.h` | M13a | Block time & sample conversion (beat → sample offset) |
| `LanePosition.h/.cpp` | M13b | Position state within lane timeline (step, beat in loop) |
| `LaneEngine.h/.cpp` | M14 | Per-lane sequencer: triggers, gate, transpose, held notes |
| `TransportTracker.h/.cpp` | M15 | Detects host transport state changes (play, stop, continuous) |
| `PassThroughFilter.h/.cpp` | M16 | Forwards held notes when no pattern triggers (§8.2) |
| `BlockTypes.h` | M17a | Block I/O: `BlockInput`, `BlockOutput`, `BlockResult` |
| `SequencerConfiguration.h/.cpp` | M17b | Global config (all lanes, pattern, voice limits) |
| `SequencerCore.h/.cpp` | M17c | Main sequencer: processes blocks, coordinates lanes, MIDI output |

## Layout

```
lib/arpeggiator/
├── LaneConfiguration.h/.cpp
├── BlockClock.h
├── LanePosition.h/.cpp
├── LaneEngine.h/.cpp
├── TransportTracker.h/.cpp
├── PassThroughFilter.h/.cpp
├── BlockTypes.h
├── SequencerConfiguration.h/.cpp
└── SequencerCore.h/.cpp
```

## Key rules

- All public types in `phu::arpeggiator` namespace
- No JUCE; samples are block-relative `SampleOffset` (int32)
- Real-time safe: preallocates, no locks, no exceptions
- Lanes independent, share global `VoiceRegistry` and output buffer

**See** [docs/architecture/04-arpeggiator-modules.md](../docs/architecture/04-arpeggiator-modules.md) (§4), [spec/README.md](../spec/README.md) (§4–7)

