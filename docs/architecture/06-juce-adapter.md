# phu-sequential — Architecture — 06-juce-adapter.md

## 6. JUCE adapter (`src/`, namespace `phu::plugin`)

### 6.1 M19 Processor, MIDI conversion, host transport — (#12)

```cpp
// HostTransport.h
namespace phu::plugin {
// [RT] Reads juce::AudioPlayHead::getPosition(): isPlaying = getIsPlaying(); playheadBeat = *getPpqPosition()
// (hasMusicalPosition = optional set); tempo = *getBpm() (0 if unset). Null playhead / nullopt -> not playing.
[[nodiscard]] arpeggiator::TransportInfo readHostTransport(juce::AudioPlayHead*) noexcept;
}

// MidiConversion.h — pure functions over preallocated storage, unit-tested in SequentialAdapterTests
namespace phu::plugin {
// Classification of one host message (D-31): note events (Note-On incl. v=0, Note-Off) and CC64 go to the core
// (CC64 additionally raw to the output); everything else (other CCs, pitch bend, aftertouch, program change,
// SysEx, realtime, …) is raw-only: copied byte-for-byte from the host input to the output at its original offset.
enum class HostEventRoute : std::uint8_t { coreOnly, coreAndRaw, rawOnly };
[[nodiscard]] HostEventRoute routeOf(const juce::MidiMessageMetadata&) noexcept;                  // [RT]

// Streams the host buffer in input order into fixed-size chunks (D-51). Holds a juce::MidiBuffer iterator, no storage
// of its own beyond the caller's fixed array; no per-host-block event ceiling.
class ChunkedInputReader {
public:
    ChunkedInputReader(const DeferredInputStore& deferred, const juce::MidiBuffer& hostInput, int numSamples) noexcept; // [RT] deferred first, at offset 0
    // Fills `chunk` (capacity inputEventsPerChunk) with the next chunk's core events (offsets relative to the chunk
    // start) and returns its descriptor; false when the host block is exhausted. Rules: D-51 / §6.1 step 3.
    struct ChunkDescriptor { int startSample; int numSamples; int eventCount; bool moreInputAtSameOffset; };
    bool next(std::span<sequencing::MidiEvent, sequencing::constants::capacity::inputEventsPerChunk> chunk,
              ChunkDescriptor&) noexcept;                                                     // [RT]
};

// Fixed-capacity output writer over the processor-owned scratch juce::MidiBuffer `outputScratch_` (D-45). Never
// allocates and never clears its target implicitly (the processor calls `beginBlock()` once per non-empty host block).
// Every append first checks the byte budget against `C = target.data.getNumAllocated()` (≥ outputMidiReserveBytes);
// eventBytes = message bytes + sizeof(int32) + sizeof(uint16) (9 for a 3-byte message). `free` = C − used bytes;
// `open` = number of (channel, pitch) whose Note-On was written and whose Note-Off was not yet written (bitset
// `outputOpen_`, 2048 bits, persists across host blocks; reset by prepareToPlay together with the core flush).
// Admission rules (D-30, D-45), all writes keep the invariant  free ≥ open · 9:
//   core Note-Off for an open key      : always written (invariant ⇒ room); open −= 1. Never refused.
//   core Note-Off for a non-open key   : suppressed (its Note-On was refused earlier, nothing sounds); not a loss.
//   core Note-On                       : written iff free − 9 ≥ noteOffReserveBytes (18 432); open += 1. Since
//                                        open ≤ 2048 = maximumSoundingVoices, the invariant holds after it.
//   raw release class (CC64 < 64,
//     CC120, CC123)                    : written iff free − 9 ≥ open · 9.
//   raw other (all other non-note msgs,
//     SysEx incl.)                     : written iff free − eventBytes ≥ noteOffReserveBytes + rawReleaseReserveBytes.
// Refused events are counted per class (droppedOutputEventsTotal) and jassert in debug builds.
class OutputMidiWriter {
public:
    OutputMidiWriter(juce::MidiBuffer& scratch, std::bitset<sequencing::constants::capacity::maximumSoundingVoices>& outputOpen) noexcept; // [RT]
    void beginBlock() noexcept;                                                                  // [RT] scratch.clear() (keeps storage)
    bool appendRaw(const juce::MidiMessageMetadata&, int sampleOffset) noexcept;                 // [RT]
    void appendCoreEvents(std::span<const sequencing::MidiEvent>, int offsetShift) noexcept;     // [RT]
    [[nodiscard]] std::uint32_t droppedEventCount() const noexcept;
};

// Zero-sample deferral store (D-53). Fixed capacity, never allocates, cumulative across consecutive zero-sample host
// blocks; emptied only by `drainInto` at the next non-empty host block. Entries keep input order. Release-class events
// (Note-Off, Note-On v=0, CC64 < 64, CC120, CC123) store their 3 bytes inline (no arena use); other events use the
// arena. Insert rules:
//   release event r with identity id (Note-Off: channel+pitch; CC64/CC120/CC123: channel+controller): any earlier
//     deferred release event with the same id is removed (subsumed: all deferred events are delivered at offset 0 of
//     one block, where the final state of id is "released" either way), then r is appended. Hence at most
//     2096 = 2048 + 3·16 release entries exist. If the store is full, the oldest non-release entry is evicted (one
//     always exists because 2096 < deferredEventCapacity = 8192) and counted. Release events are never dropped.
//   non-release event: appended iff an entry and its arena bytes are free; otherwise the new event is refused and
//     counted (earliest events are kept). Removed/evicted entries are tombstones; compaction is in place (O(entries)).
class DeferredInputStore {
public:
    void insert(const juce::MidiMessageMetadata&) noexcept;                                      // [RT]
    [[nodiscard]] bool empty() const noexcept;
    template <typename Visitor> void forEach(Visitor&&) const noexcept;                         // [RT] input order, offset 0
    void clear() noexcept;                                                                       // [RT]
    [[nodiscard]] std::uint32_t droppedEventCount() const noexcept;                              // evicted + refused
private:
    struct Entry { std::uint32_t arenaOffset; std::uint16_t size; std::uint8_t inlineBytes[3]; std::uint8_t flags; };
    std::array<Entry, sequencing::constants::capacity::deferredEventCapacity> entries_{};
    std::array<std::uint8_t, sequencing::constants::capacity::deferredArenaBytes> arena_{};
};
}

// PluginProcessor.h
namespace phu::plugin {
class SequentialAudioProcessor final : public juce::AudioProcessor, private juce::Timer {
public:
    SequentialAudioProcessor();               // BusesProperties() with no buses; MIDI in/out via JucePlugin_* flags
    void prepareToPlay(double sampleRate, int maximumBlockSize) override;  // core.reset() (D-44); outputOpen_.reset();
                                                                           // outputScratch_.ensureSize(outputMidiReserveBytes) (D-45)
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;           // [RT] bypassed = bypass param
    void processBlockBypassed(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;   // [RT] same path, bypassed = true
    juce::AudioProcessorParameter* getBypassParameter() const override;                // "bypass" (D-28)
    bool isMidiEffect() const override { return true; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }                                        // D-35
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;          // [MT or host thread] M22, D-52
    void setStateInformation(const void*, int) override;            // [MT or host thread] M22, D-52
    bool hasEditor() const override { return true; }
    juce::AudioProcessorEditor* createEditor() override;            // editor::PluginEditor

    juce::AudioProcessorValueTreeState& parameters() noexcept;      // M20
    PatternStore& patternStore() noexcept;                          // M21
    PresetManager& presetManager() noexcept;                        // M22
    [[nodiscard]] bool pollPlaybackStatus(arpeggiator::PlaybackStatus&) noexcept; // [MT] single consumer (editor timer)

private:
    void timerCallback() override;      // 30 Hz [MT]: drain step-discard mailbox into PatternStore (D-32)
    void processInternal(juce::MidiBuffer&, int numSamples, bool bypassed) noexcept; // [RT]

    juce::AudioProcessorValueTreeState parameters_;          // M20 layout incl. bypass
    ParameterReader parameterReader_;                        // cached std::atomic<float>* (M20)
    PatternStore patternStore_;                              // M21: owns PatternDocument + TripleBuffer<PatternSnapshot>
    PresetManager presetManager_;                            // M22
    arpeggiator::SequencerCore core_;                        // audio thread only (after construction)
    sequencing::OutputEventBuffer coreOutput_{sequencing::constants::capacity::outputEventsPerBlock};
    std::array<sequencing::MidiEvent, sequencing::constants::capacity::inputEventsPerChunk> inputChunk_{}; // reused
    juce::MidiBuffer outputScratch_;                         // D-45: reserved in prepareToPlay, never swapped, never grown on audio
    std::bitset<sequencing::constants::capacity::maximumSoundingVoices> outputOpen_; // D-45: written Note-On without Note-Off
    DeferredInputStore deferredInput_;                       // D-53: cumulative events of zero-sample host blocks (in-object)
    std::uint32_t hostOutputGrowthCount_ = 0;                // D-54: audio-thread growths of the host buffer (deviation, A-43)
    sequencing::TripleBuffer<arpeggiator::PlaybackStatus> playbackStatus_;
    sequencing::TripleBuffer<arpeggiator::StepDiscardState> stepDiscards_;  // D-32: audio -> processor timer
    // D-52: complete versioned state (parameters + patterns) from setStateInformation / preset load -> audio.
    struct PendingState { std::uint64_t generation; arpeggiator::SequencerConfiguration parameters;
                          arpeggiator::PatternSnapshot patterns; };
    sequencing::TripleBuffer<PendingState> pendingState_;
    std::mutex stateMutex_;                                  // [non-audio] serializes state producers (never audio)
    std::uint64_t stateGeneration_ = 0;                      // guarded by stateMutex_
    std::atomic<std::uint64_t> latestStateGeneration_{0};    // = stateGeneration_ after each publish (read by callAsync callbacks)
    std::atomic<std::uint64_t> parametersSyncedGeneration_{0}; // APVTS reflects generation g; advanced only by CAS-max (D-52)
    std::uint64_t apvtsAppliedGeneration_ = 0;               // message thread only: last generation written into APVTS
    std::uint64_t audioStateGeneration_ = 0;                 // audio only: last applied PendingState generation
    arpeggiator::SequencerConfiguration stateParameters_{};  // audio only: used while APVTS lags (D-52)
    std::uint32_t droppedOutputEventsTotal_ = 0;
};
}
```

`processInternal` (audio thread), per host block (`n = numSamples`):

0. If `n == 0` (D-53): no core call and the writer/scratch is not touched (no target is cleared). Every event of `midi` is inserted, in input order, into `deferredInput_` (`DeferredInputStore` rules: cumulative across consecutive zero-sample blocks, release-class events never dropped, evictions/refusals of non-release events counted); then `midi.clear()` (keeps storage, no allocation). Nothing is emitted; all core state (pending Note-Offs, transitions, flushes) stays pending.
1. **State and parameters**: if `pendingState_.update()` and its `generation > audioStateGeneration_`: `audioStateGeneration_ = generation`, `stateParameters_ = parameters`, `core_.setPatterns(patterns)` (D-52). `cfg = (parametersSyncedGeneration_.load(acquire) < audioStateGeneration_) ? stateParameters_ : buildConfiguration(parameterReader_.read())` — the loaded parameters and patterns become active in the **same** block and stay active until APVTS has caught up. Then, if `patternStore_.audioSide().update()` → `core_.setPatterns(read())` (older document revisions are ignored by the core).
2. `transport = readHostTransport(getPlayHead())`; `OutputMidiWriter writer(outputScratch_, outputOpen_)`; `writer.beginBlock()`. The host buffer `midi` is only **read** in steps 2–4. **Pass 1 (raw, D-31)**: iterate `deferredInput_` (offset 0) then `midi`; every `rawOnly`/`coreAndRaw` event → `writer.appendRaw` at its original offset. Non-note events are never filtered by Thru/Consume/bypass and never enter a core queue.
3. **Pass 2 (chunks, D-51)**: `ChunkedInputReader` over `deferredInput_` + `midi` (core-routed events only, offsets clamped to `[0, n−1]`). Chunk rules: a chunk `[a, b)` is cut only between complete sample-offset groups, so every event offset of the chunk lies in `[a, b)`; `b − a ≤ samplesPerChunk`; ≤ `inputEventsPerChunk` events; the last chunk ends at `n` (a chunk may have no events). If one offset group at `s` alone exceeds `inputEventsPerChunk`: the preceding chunk ends at `s`; the group is split, in input order, into pieces of ≤ `inputEventsPerChunk` events; every piece but the last is a core call starting at `s` with `moreInputAtSameOffset = true`; the last piece starts the regular chunk `[s, b)`. Nothing is dropped and there is no per-host-block event ceiling.
4. Per core call: `input.transport.playheadBeat += a / samplesPerBeat`, offsets relative to `a`; `result = core_.processBlock(input, cfg, coreOutput_)`; `writer.appendCoreEvents(coreOutput_.events(), a)` (drained after every call); if `result.discardStateChanged` → remember to publish.
5. **Copy to host (D-45, D-54)**: `needed = outputScratch_.data.size()`; `midi.clear()` (keeps storage). If `midi.data.getNumAllocated() < needed` → growth path: `midi.ensureSize(outputScratch_.data.getNumAllocated())`, `++hostOutputGrowthCount_` (the only audio-thread allocation of the plugin; known deviation from §17, A-43). Then `midi.addEvents(outputScratch_, 0, -1, 0)` — implemented as the byte-identical append `midi.data.addArray(outputScratch_.data)` (scratch is already sorted and well-formed; avoids `addEvent`'s per-event insertion search; within capacity it does not allocate). `deferredInput_.clear()`. `playbackStatus_.write(core_.playbackStatus())` (with `droppedOutputEventsTotal_ += writer.droppedEventCount() + deferredInput_.droppedEventCount()` and `hostOutputGrowthCount_`); if a discard changed: `stepDiscards_.write(core_.stepDiscardState())`.

Output buffer (D-45, D-54). No `swapWith`: the processor keeps exactly one output store, `outputScratch_`, reserved with `ensureSize(outputMidiReserveBytes)` in `prepareToPlay` (message thread) and never exchanged, so its capacity cannot erode. The host buffer is a host-owned `juce::MidiBuffer` whose storage is a `juce::Array<uint8>`; the processor cannot reach it before the first `processBlock`, and any growth of it allocates. The only zero-allocation guarantee available is therefore "the merged output fits in the host buffer's existing storage". Policy when it does not: grow the host buffer once, directly to the full scratch capacity, on the audio thread, and count it. Rationale: truncating instead could drop Note-Offs (stuck notes, violating §9.3), which is worse than one allocation; JUCE plugin wrappers reuse one `MidiBuffer` object per instance and `clear()` keeps its storage, so in practice the growth happens at most once per host buffer object (typically in the first dense block) and never again; a host that hands a fresh small buffer every block would grow every such block — visible in `PlaybackStatus::hostOutputGrowthCount`. Tagged as a known deviation from §17, raised for the owner as A-43; the RT test asserts 0 allocations once the host buffer has the reserved capacity and exactly 1 for a first dense block into a small buffer.

What can and cannot be dropped (C = 1 048 576 scratch bytes, 9 bytes per 3-byte event; `OutputMidiWriter` rules):
- Core input (note/CC64): never dropped (D-51 chunking, no per-host-block ceiling).
- Core output buffer: never refuses anything (§5.2 bound 13 376 ≤ 14 336 general slots; Note-Offs own 2048 reserved slots, M06).
- Generated Note-Offs (lane and pass-through, incl. stop/bypass/flush releases) in the host output: never dropped. Invariant `free ≥ open·9`, `open ≤ 2048`, worst case 2048·9 = 18 432 bytes (`noteOffReserveBytes`) that raw events and Note-Ons can never consume.
- Raw release-class input (CC64 < 64, CC120, CC123): written while `free − 9 ≥ open·9`, i.e. at least (C − 18 432)/9 = 114 460 three-byte events per host block are accepted; beyond that (or fewer if large SysEx precede) they **can** be dropped and are counted — A-44.
- Raw other events and core Note-Ons: written while `free − bytes ≥ 18 432 + 432`, i.e. ≥ (C − 18 864)/9 = 114 412 three-byte events per host block; beyond that they **can** be dropped and are counted. A refused Note-On marks nothing open, so its later core Note-Off is suppressed (no orphan, no stuck note), but the core still counts that voice toward its limits until it ends — A-44.
- Deferred input of zero-sample blocks: release-class events never dropped; non-release events can be evicted/refused beyond 8192 entries or 65 536 arena bytes, counted (D-53).
- Copy to host: never drops (growth path instead, D-54).

Processor timer (30 Hz, message thread): `if (stepDiscards_.update()) patternStore_.applyDiscardState(stepDiscards_.read())` (D-32); playback status is consumed by the editor (`pollPlaybackStatus`).

### 6.2 M20 Parameters — `ParameterIds.h`, `ParameterLayout.h/.cpp`, `ParameterReader.h/.cpp` (+ core part §4.7) (#13)

ID scheme: global IDs are plain camelCase; lane IDs are `lane<N>_<name>` with **N = 1..8** (user-visible lane numbers). All IDs use `juce::ParameterID{id, 1}` (version hint 1, required by AU/VST3 wrappers; bump only for newly added parameters). Display names: global as in §12 ("Thru", "Consume", "Chord Input Channel"); lane "Lane N <Name>" (e.g. "Lane 3 Swing"). Every parameter in the table is automatable (§12); there are no hidden/non-automatable host parameters except `bypass` (host bypass, also automatable per host convention). Total: **115 spec parameters** (3 global + 8 × 14 lane, §12) plus 1 host bypass parameter = **116 host parameters**; `bypass` is outside the §12 count and is not persisted (D-28).

| ID | Type (JUCE) | Range / choices (0-based index order) | Default (§20) | Unit / note |
|---|---|---|---|---|
| `thru` | `AudioParameterBool` | off/on | off | §3.3 |
| `consume` | `AudioParameterBool` | off/on | on | §3.3 |
| `chordInputChannel` | `AudioParameterInt` | 1..16 | 1 | MIDI channel, §3.2 |
| `lane<N>_mute` | `AudioParameterBool` | off/on | off | §4.6 |
| `lane<N>_latch` | `AudioParameterBool` | off/on | off | §4.6 |
| `lane<N>_inputMode` | `AudioParameterChoice` | Chord Follow, Channel | Chord Follow | from `inputModeRegistry` |
| `lane<N>_followIndex` | `AudioParameterInt` | 0..15 | 0 | §4.2 (0 = lowest) |
| `lane<N>_followChannel` | `AudioParameterInt` | 1..16 | 1 | §4.3, D-33 |
| `lane<N>_outputChannel` | `AudioParameterInt` | 1..16 | 1 | §8 |
| `lane<N>_transpose` | `AudioParameterInt` | −24..24 | 0 | semitones, §8 |
| `lane<N>_gate` | `AudioParameterInt` | 1..200 | 100 | percent, §6 (D-7) |
| `lane<N>_timingSource` | `AudioParameterChoice` | Base Timing, Text Grid | Base Timing | from `timingSourceRegistry` |
| `lane<N>_baseTiming` | `AudioParameterChoice` | 1/32, 1/16, 1/8, 1/4, 1/2, 1/1 | 1/16 (index 1) | from `baseTimingRegistry()` |
| `lane<N>_subdivision` | `AudioParameterChoice` | Straight, Triplet, Dotted | Straight | from `subdivisionRegistry` |
| `lane<N>_swing` | `AudioParameterInt` | 0..100 | 0 | percent, §5.1 (D-7) |
| `lane<N>_stepCount` | `AudioParameterInt` | 1..64 | 16 | §5.4 |
| `lane<N>_velocityMode` | `AudioParameterChoice` | Incoming, By Step, Delta by Step, Relative | Incoming | from `velocityModeRegistry` |
| `bypass` | `AudioParameterBool` | off/on | off | host bypass (§21); not saved in state/presets (D-28) |

All ranges/defaults/choice lists are generated from `constants` and the registries — no literals in `ParameterLayout.cpp`. Choice *indices* equal enum values (static_asserted); state files store `persistenceKey` strings, not indices (D-34).

```cpp
// ParameterIds.h — the only place spelling IDs
namespace phu::plugin::parameter_ids {
inline constexpr std::string_view thru = "thru", consume = "consume", chordInputChannel = "chordInputChannel",
                                   bypass = "bypass";
enum class LaneParameter : std::uint8_t { mute, latch, inputMode, followIndex, followChannel, outputChannel,
    transpose, gate, timingSource, baseTiming, subdivision, swing, stepCount, velocityMode };
inline constexpr int laneParameterCount = 14;
inline constexpr std::array<std::string_view, laneParameterCount> laneParameterSuffixes{
    "mute", "latch", "inputMode", "followIndex", "followChannel", "outputChannel", "transpose", "gate",
    "timingSource", "baseTiming", "subdivision", "swing", "stepCount", "velocityMode"};
[[nodiscard]] juce::String laneParameterId(int laneIndex0, LaneParameter);   // "lane" + (laneIndex0+1) + "_" + suffix
[[nodiscard]] juce::String laneParameterName(int laneIndex0, LaneParameter); // "Lane 1 Follow Index"
inline constexpr int parameterVersionHint = 1;
}
// ParameterLayout.h
namespace phu::plugin {
[[nodiscard]] juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();   // [MT]
// ParameterReader.h — caches raw value pointers once; read() is lock-free
class ParameterReader {
public:
    explicit ParameterReader(juce::AudioProcessorValueTreeState&);  // [MT] getRawParameterValue for each ID (jassert non-null)
    [[nodiscard]] arpeggiator::ParameterSnapshot read() const noexcept;   // [RT] relaxed atomic loads, round to int
private:
    std::atomic<float>* thru_; std::atomic<float>* consume_; std::atomic<float>* chordInputChannel_;
    std::array<std::array<std::atomic<float>*, parameter_ids::laneParameterCount>, sequencing::constants::lanes::laneCount> lanes_;
};
}
```

Values are read **once per block** (D-21): a parameter change becomes visible at offset 0 of the next processed block; the core's §11 rules then decide when it takes effect. Sample-accurate automation inside a block is not used (JUCE does not provide it for APVTS).

### 6.3 M21 Pattern store — `PatternStore.h/.cpp` (+ core part §4.8) (#14)

Owner of all non-parameter state on the non-audio side and the single producer of `PatternSnapshot` for the audio thread (D-32).

```cpp
namespace phu::plugin {
class PatternStore : public juce::ChangeBroadcaster {     // change messages delivered async on the message thread
public:
    PatternStore();                                        // default document, snapshot published
    // [MT/any non-audio thread] Every mutator: lock mutex_, mutate document_, publish snapshot, sendChangeMessage().
    void setStepEnabled(int lane, int step, bool enabled);
    void setStepValue(int lane, int step, sequencing::VelocityMode, int value);
    sequencing::TextGridParseResult setTextGridText(int lane, std::string text);   // result drives red marking
    void setLaneColor(int lane, arpeggiator::LaneColor);
    // [MT] processor timer, from the audio->message StepDiscardState triple buffer (D-32): document_.applyDiscardState
    // resets only slots not edited after the discard, acknowledges the lane's discard revision, publishes.
    void applyDiscardState(const arpeggiator::StepDiscardState&);
    // state/preset load (D-52): the new document gets revision > every revision published so far; returns the
    // published snapshot so the caller can embed it in the processor's PendingState.
    arpeggiator::PatternSnapshot replaceDocument(arpeggiator::PatternDocument);
    [[nodiscard]] arpeggiator::PatternDocument copyDocument() const;   // state/preset save, editor refresh
    // Audio-thread consumer side ONLY (update()/read()).
    [[nodiscard]] sequencing::TripleBuffer<arpeggiator::PatternSnapshot>& audioSide() noexcept;
private:
    void publishLocked();                                  // toAudio_.write(document_.makeSnapshot())
    mutable std::mutex mutex_;                             // never touched by the audio thread
    arpeggiator::PatternDocument document_;
    sequencing::TripleBuffer<arpeggiator::PatternSnapshot> toAudio_;
};
}
```

Step-data edit latency: an edit is visible to the audio thread at the next block start after `publish()`; the core treats it like a parameter change (step data is read at trigger time, §11). Every snapshot carries `documentRevision` (monotonic, D-32); the core ignores snapshots older than the one it holds.

**#12 stub vs #14** (§9): #12 ships `src/PatternStore.h/.cpp` with exactly this minimal API — `PatternStore()` (default `PatternDocument`, revision 1, snapshot published), `TripleBuffer<PatternSnapshot>& audioSide() noexcept`, `void applyDiscardState(const StepDiscardState&)` (stub: no-op; the core keeps masking until acknowledged, D-32) — and no `ChangeBroadcaster` base. #14 keeps these three signatures unchanged, replaces `PatternStore.cpp`, adds the `juce::ChangeBroadcaster` base, the mutex, all mutators, `replaceDocument`, `copyDocument`, and the real `applyDiscardState`.

### 6.4 M22 State, presets, versioning — `StateSerializer`, `StateMigration`, `PresetManager` (#14)

**Format (D-34)** — JUCE `ValueTree` serialized as XML (§13). Same schema for host state and preset files; `kind` distinguishes them. Host state is stored with `AudioProcessor::copyXmlToBinary` (VST3 state); preset files are UTF-8 XML text.

```xml
<PhuSequential formatVersion="1" kind="state">            <!-- kind: "state" | "preset" -->
  <PresetInfo name="Basic Up"/>                            <!-- presets only; ignored in state -->
  <Global thru="0" consume="1" chordInputChannel="1"/>
  <Lane index="1" mute="0" latch="0" inputMode="chordFollow" followIndex="0" followChannel="1"
        outputChannel="1" transpose="0" gate="100" timingSource="baseTiming" baseTiming="1/16"
        subdivision="straight" swing="0" stepCount="16" velocityMode="incoming" color="#4FC3F7">
    <TextGrid text="1/8t, 1/8., 1/4" applied="1/8t, 1/8., 1/4"/>   <!-- text: as typed; applied: last valid (D-9) -->
    <Steps on="1,1,1,…(64)" byStep="100,…(64)" deltaByStep="0,…(64)" relative="100,…(64)"/>
  </Lane>
  <!-- … Lane index 2..8 … -->
</PhuSequential>
```

Rules:
- Attribute names = parameter suffixes (§6.2); choice values = registry `persistenceKey` (base timing: its name, e.g. `1/16`); booleans `0/1`; ints decimal; color `#RRGGBB`. `<Steps>` has one CSV attribute per velocity mode with `usesStepValues` (named by `persistenceKey`) plus `on`; always 64 values.
- `bypass` is never written (D-28). Lane color is state only, not a parameter (§12).
- **Load** (`StateSerializer::load`): parse → root tag/`formatVersion` attribute missing or XML unparsable → `rejectedCorrupt`; `formatVersion > current` → `rejectedNewerVersion`; both fall back to the §20 default state (§13) and are reported (no exception escapes). `formatVersion < current` → apply the migration chain one version at a time → `migrated`. Then read every known field: unknown attributes/children ignored; missing fields → §20 default; unparsable or out-of-range values → clamped (numeric) or default (keys) → `loadedWithClamping`. CSV with fewer than 64 values → remaining defaults; more → ignored. `applied` is re-parsed; invalid → empty grid. Then I-1 is enforced against the loaded effective step count.
- **Apply** (D-52, atomic): `setStateInformation` / preset load may run on the message thread or another non-audio host thread. It parses into one `PluginStateData`, then under `stateMutex_`: `patterns = patternStore_.replaceDocument(document)`; `generation = ++stateGeneration_`; `latestStateGeneration_.store(generation, release)`; `pendingState_.write({generation, parameters, patterns})` — one complete versioned snapshot (parameters + patterns) for the audio thread, which adopts both together at the start of its next host block (§6.1 step 1; no reset; §11 timing applies) and uses the snapshot's parameters until APVTS has caught up. APVTS and UI are updated separately by `syncParametersToApvts(generation, parameters)`, run on the message thread (via `juce::MessageManager::callAsync`, or directly when already on the message thread). JUCE does not guarantee that callbacks posted from different threads run in posting order, so every callback carries its generation `g` and is generation-guarded: (1) if `g < latestStateGeneration_.load(acquire)` or `g <= apvtsAppliedGeneration_` → return immediately (superseded; the newer callback will sync); (2) otherwise run `param->setValueNotifyingHost(param->convertTo0to1(value))` for all parameters, set `apvtsAppliedGeneration_ = g`; (3) advance `parametersSyncedGeneration_` monotonically by CAS-max (`cur = load(); while (cur < g && !compare_exchange_weak(cur, g, release, relaxed)) {}`), so it can never move backwards. A stale callback therefore can neither overwrite newer APVTS values nor lower the synced generation. A later host automation value wins only after that sync, never by mixing old and new state within one block. No claim of "any thread" safety is made beyond this: `stateMutex_` serializes concurrent producers, `PatternStore` has its own mutex, and the audio thread only reads triple buffers. `getStateInformation` reads `APVTS::copyState()` (JUCE: thread-safe) and `PatternStore::copyDocument()` (mutex).

```cpp
namespace phu::plugin {
enum class StateKind : std::uint8_t { state, preset };
enum class LoadOutcome : std::uint8_t { loaded, migrated, loadedWithClamping, rejectedNewerVersion, rejectedCorrupt };
struct PluginStateData {                                     // JUCE-free payload
    arpeggiator::SequencerConfiguration parameters = arpeggiator::defaultSequencerConfiguration();
    arpeggiator::PatternDocument document;
};
struct LoadResult { LoadOutcome outcome; PluginStateData data; };   // data = defaults when rejected

// StateMigration.h — one function per version step: tree in version v -> version v+1.
using MigrationFunction = juce::ValueTree (*)(juce::ValueTree);
// Production chain; index v-1 migrates v -> v+1. Empty while currentFormatVersion == 1.
[[nodiscard]] std::span<const MigrationFunction> productionMigrations() noexcept;
// Chain injectable so tests can exercise migration with a synthetic old version (issue #14).
[[nodiscard]] std::optional<juce::ValueTree> migrateToVersion(juce::ValueTree, int targetVersion,
                                                              std::span<const MigrationFunction> chain);

// StateSerializer.h  [MT]
[[nodiscard]] juce::ValueTree toValueTree(const PluginStateData&, StateKind, const juce::String& presetName = {});
[[nodiscard]] LoadResult fromValueTree(const juce::ValueTree&,
                                       std::span<const MigrationFunction> chain = productionMigrations(),
                                       int currentVersion = sequencing::constants::state::currentFormatVersion);
[[nodiscard]] LoadResult fromXmlText(const juce::String&);
[[nodiscard]] PluginStateData captureState(const juce::AudioProcessorValueTreeState&, const PatternStore&);
void applyState(const PluginStateData&, juce::AudioProcessorValueTreeState&, PatternStore&);

// PresetManager.h  [MT]
struct PresetInfo {
    juce::String name;
    bool isFactory = false;
    int factoryIndex = -1;          // into the BinaryData factory list
    juce::File file;                // user presets
};
class PresetManager {
public:
    PresetManager(juce::AudioProcessorValueTreeState&, PatternStore&);
    [[nodiscard]] static juce::File userPresetDirectory();   // userApplicationDataDirectory/phu/phu-sequential/Presets
    [[nodiscard]] std::vector<PresetInfo> listPresets() const; // factory (fixed order) then user (*.phuseq, case-insensitive name order)
    LoadOutcome loadPreset(const PresetInfo&);                // same path as state load (fromXmlText + applyState)
    bool saveUserPreset(const juce::String& name);            // kind="preset"; file name sanitized; overwrites same name
    LoadOutcome loadNext();                                   // wraps around listPresets()
    LoadOutcome loadPrevious();
    [[nodiscard]] juce::String currentPresetName() const;     // last loaded/saved name; "" initially; no dirty tracking
};
}
```

D-35: user presets are `*.phuseq` files in `juce::File::userApplicationDataDirectory/phu/phu-sequential/Presets` (created on first save). Host program API is not used (`getNumPrograms() == 1`). A rejected preset load (newer version / corrupt) applies the §20 default state exactly like a rejected host-state load (§13 makes no exception for presets) and the editor shows the outcome (D-47).

### 6.5 M23 Editor — `src/editor/*` (+ core part §4.9) (#15)

```cpp
namespace phu::plugin::editor {
class PluginEditor final : public juce::AudioProcessorEditor, private juce::Timer, private juce::ChangeListener {
public:
    explicit PluginEditor(SequentialAudioProcessor&);   // size defaultWidth x defaultHeight, resizable with limits (D-36)
    void resized() override;
    void paint(juce::Graphics&) override;
private:
    void timerCallback() override;                       // statusRefreshHz: pollPlaybackStatus, recompute layouts/scale
    void changeListenerCallback(juce::ChangeBroadcaster*) override;   // PatternStore changed: refresh lanes
    SequentialAudioProcessor& processor_;
    EditorLookAndFeel lookAndFeel_;
    GlobalBar globalBar_;
    juce::Viewport laneViewport_;                         // vertical scrolling of the 8 rows (§14)
    juce::Component laneContainer_;
    std::array<std::unique_ptr<LaneRow>, sequencing::constants::lanes::laneCount> laneRows_;
    juce::ScrollBar stepAreaScrollBar_{false};            // ONE horizontal bar shared by all step areas (§14)
    arpeggiator::DisplayScale scale_;
};
class GlobalBar : public juce::Component {               // Thru, Consume (toggle + ButtonAttachment),
    // Chord Input Channel (ComboBox 1..16 + attachment), preset browser: name label, ◂ ▸, Save…, Load… (PresetManager)
};
class LaneRow : public juce::Component {                 // height laneRowHeight
    // LaneControlStrip | (TextGridField above StepDisplay) ; lane number badge in lane color
};
class LaneControlStrip : public juce::Component {
    // Mute, Latch (toggles); Input Mode (combo); Follow Index (shown in Chord Follow) / Follow Channel (shown in
    // Channel) (D-46); Output Channel; Transpose; Gate; Timing Source (2-segment switch "Base | Grid");
    // Base Timing, Subdivision, Swing, Step Count (disabled while Timing Source = Text Grid, §14); Velocity Mode;
    // LaneColorPicker. All via APVTS attachments created from parameter_ids.
};
class StepDisplay : public juce::Component {
    // Draws steps at DisplayScale geometry (x offset = scrollbar position); pads: on = lane color, off = neutral
    // (§14); velocity drag bar per step: range = velocityModeDefinition(mode).stepValueRange, value shown while
    // dragging, disabled when !usesStepValues; click pad -> PatternStore::setStepEnabled; drag -> setStepValue.
    // Playback indicator: amber frame on LaneStatus::currentStepIndex while running.
};
class TextGridField : public juce::Component {
    // juce::TextEditor; on every text change: PatternStore::setTextGridText; for each result.errors entry draw a
    // red overlay over the character range [textBegin, textEnd) (TextEditor::getTextBounds(Range)) (§5.3).
    // Editable regardless of Timing Source (§14). An empty applied grid shows a "no grid" placeholder (D-9).
};
class LaneColorPicker : public juce::Component {
    // Swatch button; click opens juce::CallOutBox with juce::ColourSelector (no alpha); change ->
    // PatternStore::setLaneColor(lane, rgb). Affects only this lane (§14).
};
}
```

Editor rules:
- **Thread**: message thread only. Reads parameters through attachments; reads/writes pattern data only via `PatternStore` (mutex, never the audio thread); reads playback through `pollPlaybackStatus` (wait-free). No editor code runs on the audio thread; closing the editor never affects audio (§17).
- **Scale** (§14): each timer tick and on parameter/document change, build `makeActiveGridLayout(lane params read from APVTS, applied grid)` for 8 lanes → `computeDisplayScale(layouts, stepAreaWidth)`; if `contentWidthPixels > stepAreaWidth`, the shared scrollbar is visible.
- **Size** (D-36): default 1280×820, min 1000×420, max 3840×2400, `setResizable(true, true)` + `setResizeLimits`. Rows are fixed height (`laneRowHeight` 140 px), so 8 rows = 1120 px and the viewport scrolls vertically below that. Step-area width = editor width − `controlStripWidth` (148 px, from the sketch) − margins; minimum step width 16 px (D-37).
- Colors: lane palette from `constants::colors::defaultLanePalette` (§20); neutral off-step color and amber indicator are `EditorLookAndFeel` colours (UI only, not spec constants).

### 6.6 M24 Factory presets — `presets/factory/*.phuseq` (#16)

Embedded via the conditional `SequentialFactoryPresets` binary-data target of §8.2 (globbed `presets/factory/*.phuseq`; file order = list order, sorted by a numeric file-name prefix `NN-Name.phuseq`). `juce_add_binary_data` fails with an empty source list, so the target is only created when at least one preset exists; otherwise `PHU_HAS_FACTORY_PRESETS=0` and `PresetManager` exposes an empty factory list (`#if PHU_HAS_FACTORY_PRESETS`). Hence #4 and every issue before #16 build without preset files. Each file is `kind="preset"`, `formatVersion` = current, contains every field (no clamping/migration on load — tested). Planned set (§13 "useful built-in presets"; content authored in #16 with a one-line description in `presets/factory/README.md`):

| File | Content |
|---|---|
| `01-Init.phuseq` | exactly §20 defaults |
| `02-Basic-Up.phuseq` | lanes 1–4 chord-follow index 0..3, 1/16, step count 4, lane *k* has only step *k* on → 4-note up arpeggio |
| `03-Channel-Bass.phuseq` | lane 1 channel mode (follow channel 2), 1/8, transpose −12, by-step accents |
| `04-Text-Grid-Rhythm.phuseq` | lane 1 text grid `1/8., 1/8., 1/8` with delta velocity |
| `05-Multi-Lane-Layered.phuseq` | several lanes with different step counts (polymetric), output channels 1–3 |

---

