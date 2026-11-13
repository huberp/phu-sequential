# src — JUCE plugin adapter

**Namespace**: `phu::plugin`

## What exists today

| File | Purpose | Status |
|------|---------|--------|
| `PluginProcessor.h/.cpp` | JUCE `AudioProcessor` subclass: host transport, MIDI I/O, state serialization, editor creation. | **Core only** (#12 stub) |
| `PluginEditor.h/.cpp` | JUCE `AudioProcessorEditor` subclass: placeholder UI. | **Stub** (minimal) |

## What is planned

- **#13**: Parameter system (IDs, layout, readers), configuration builder
- **#14**: Pattern store (document, TripleBuffer<PatternSnapshot>, preset/state serialization)
- **#15**: Editor UI (display scale, step grid, lane panes)
- **#16**: Factory presets

## Rules

- **JUCE Adapter Only**: `src/` is the only place JUCE headers are allowed
- Uses `phu::plugin` namespace for all plugin-specific types
- Bridges JUCE I/O to `phu::arpeggiator::SequencerCore` (portable core)
- No core sequencing logic; core lives in `lib/`
- Real-time safe: `processBlock` calls `SequencerCore::processBlock`, which preallocates and never locks

## Key types

- `SequencerCore`: the real-time sequencer (from `lib/arpeggiator`)
- `BlockInput`, `BlockOutput`, `BlockResult`: portable block I/O (from `lib/arpeggiator`)
- JUCE: `juce::AudioProcessor`, `juce::MidiBuffer`, `juce::AudioBuffer<float>`

**See** `docs/architecture.md` (grep `-n '^### 6.'`), `spec/arp-spec.md` (§6–14) for architecture and feature roadmap.
