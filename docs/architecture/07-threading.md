# phu-sequential — Architecture — 07-threading.md

## 7. Threading model (§17)

### 7.1 Threads and ownership

| Thread | Owns / may touch | Must not touch |
|---|---|---|
| **Audio** (`processBlock`) | `SequencerCore` (exclusive after `prepareToPlay`), `coreOutput_`, `inputChunk_`, `outputScratch_`, `outputOpen_`, `deferredInput_`, consumer side of `PatternStore::audioSide()` and `pendingState_`, producer side of `playbackStatus_` and `stepDiscards_`, relaxed loads of APVTS raw atomics, acquire load of `parametersSyncedGeneration_` | `PatternStore` mutex/document, `stateMutex_`, `PresetManager`, any `juce::ValueTree`, editor |
| **Message** (UI, timers) | Editor, `PatternStore` mutators (mutex), `PresetManager`, `setStateInformation`/`getStateInformation` when the host calls them there, APVTS `setValueNotifyingHost` for state loads (D-52), processor `Timer` (consumer of `stepDiscards_`), consumer side of `playbackStatus_` | `SequencerCore`, triple-buffer consumer side of patterns / pending state |
| **Host other threads** (some hosts call get/setStateInformation off the message thread) | only the D-52 path: parse, `PatternStore` (its mutex), producer side of `pendingState_` under `stateMutex_`, `MessageManager::callAsync` for APVTS/UI; `getStateInformation`: `APVTS::copyState()` + `PatternStore::copyDocument()` | APVTS parameter setters, editor, `SequencerCore` |

`prepareToPlay`/`releaseResources` run while audio is stopped (JUCE contract), so `core_.reset()` there is safe.

### 7.2 Communication paths

| Path | Mechanism | Payload | Latency |
|---|---|---|---|
| Host/editor → audio: parameters | APVTS `std::atomic<float>` per parameter; `ParameterReader::read()` once per block | `ParameterSnapshot` → `SequencerConfiguration` (by value, on the stack/member) | next block (D-21) |
| Editor/state → audio: pattern data | `TripleBuffer<PatternSnapshot>`; single producer = `PatternStore` under its mutex; consumer = audio thread | 8 × `LanePattern` (≈ 10 KB, trivially copyable) | next block |
| Audio → editor: playback | `TripleBuffer<PlaybackStatus>`; producer audio, consumer editor timer | lane step indices, running, latch, transport, drops | ≤ 1/30 s |
| Audio → message: step-tail discards (D-32) | `TripleBuffer<StepDiscardState>`; producer audio (written when `BlockResult::discardStateChanged`), consumer processor timer → `PatternStore::applyDiscardState` | per lane: `discardRevision`, `discardedAtRevision[64]` | ≤ 1/30 s; until acknowledged by a snapshot the core masks the slots itself |
| State load → audio: complete state (D-52) | `TripleBuffer<PendingState>`; producer `setStateInformation`/preset load under `stateMutex_`; consumer audio at host-block start | `{generation, SequencerConfiguration, PatternSnapshot}` | next block, parameters and patterns together |
| State load → message: APVTS sync (D-52) | `callAsync` callbacks carrying generation `g`; stale `g` returns early; `parametersSyncedGeneration_` advanced by CAS-max, read by audio (acquire) | parameter values of generation `g` | message-thread latency; audio uses snapshot parameters meanwhile |

No lock-free *queue* is needed: all audio-visible data is "latest value" state; the discard state is cumulative and revisioned, so only the latest value matters.

### 7.3 Preallocated structures (no allocation on the audio thread; sole exception: host output growth, D-54/A-43)

| Structure | Owner | Capacity | When allocated |
|---|---|---|---|
| `OutputEventBuffer` | processor | `outputEventsPerBlock` = 16384 events, of which `noteOffReserveEvents` = 2048 usable by Note-Offs only (D-30) | constructor |
| `inputChunk_` (fixed array, reused per chunk) | processor | `inputEventsPerChunk` = 4096 events; no per-host-block limit (D-51) | in-object |
| `VoiceRegistry` slots + scheduled-end heap | core (by value) | 2048 = 16 ch × 128 pitches (max possible simultaneous voices) | in-object |
| Held notes, pass-through tables | core | 16 × 128 | in-object |
| Lanes, grids, step data | core | 8 lanes × 64 steps | in-object |
| `TripleBuffer<PatternSnapshot>`, `TripleBuffer<PlaybackStatus>`, `TripleBuffer<StepDiscardState>`, `TripleBuffer<PendingState>` | `PatternStore`, processor | 3 slots | in-object |
| `outputScratch_` (`juce::MidiBuffer`) | processor | `outputMidiReserveBytes` = 1 MiB, of which `noteOffReserveBytes` = 18 432 for core Note-Offs and `rawReleaseReserveBytes` = 432 for raw release events (D-45) | `ensureSize` in `prepareToPlay` (message thread); never swapped or grown on the audio thread |
| `outputOpen_` (bitset) | processor | 2048 bits | in-object |
| `deferredInput_` (`DeferredInputStore`) | processor | `deferredEventCapacity` = 8192 entries + `deferredArenaBytes` = 64 KiB (D-53) | in-object |
| Host output `juce::MidiBuffer` | host | host's existing storage | **not** preallocatable by the plugin; grown once on the audio thread to the scratch capacity only if too small — known deviation from §17 (D-54, A-43) |

`SequencerCore` is a member of the processor (≈ 140 KB; the processor is heap-allocated by the host wrapper, so no stack concern). Unit test `[REQ-17-…][rt]` wraps `processBlock` with `tests/support/AllocationGuard` (counts global `operator new` calls in scope; must stay 0) over scenario runs covering every path (start/stop/locate/bypass/max events).

---

