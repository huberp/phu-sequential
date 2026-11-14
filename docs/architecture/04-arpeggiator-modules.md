# phu-sequential — Architecture — 04-arpeggiator-modules.md

## 4. Plugin-specific core (`lib/arpeggiator`, namespace `phu::arpeggiator`)

### 4.1 M12 Lane configuration — `LaneConfiguration.h/.cpp` (#10)

```cpp
namespace phu::arpeggiator {
using namespace phu::sequencing; // (sketch only; real headers use qualified names)

enum class TimingSource : std::uint8_t { baseTiming = 0, textGrid = 1 };    // §5.2
inline constexpr int timingSourceCount = 2;
struct TimingSourceDefinition { std::string_view name; std::string_view persistenceKey; };
inline constexpr std::array<TimingSourceDefinition, timingSourceCount> timingSourceRegistry{{
    {"Base Timing", "baseTiming"}, {"Text Grid", "textGrid"}}};

// Host parameters of one lane (§12), already validated (see clampLaneParameters). Defaults = §20.
struct LaneParameters {
    bool mute = false;
    bool latch = false;
    InputMode inputMode = InputMode::chordFollow;
    int followIndex = constants::lanes::defaultFollowIndex;       // 0..15
    int followChannel = constants::lanes::defaultFollowChannel;   // 1..16
    int outputChannel = constants::lanes::defaultOutputChannel;   // 1..16
    int transpose = constants::lanes::defaultTranspose;           // -24..24
    int gatePercent = constants::lanes::defaultGatePercent;       // 1..200
    TimingSource timingSource = TimingSource::baseTiming;
    int baseTimingIndex = constants::timing::defaultBaseTimingIndex; // 0..5 into baseTimingRegistry
    Subdivision subdivision = Subdivision::straight;
    int swingPercent = constants::lanes::defaultSwingPercent;     // 0..100
    int stepCount = constants::lanes::defaultStepCount;           // 1..64 (base timing only)
    VelocityMode velocityMode = VelocityMode::incoming;
    friend constexpr bool operator==(const LaneParameters&, const LaneParameters&) noexcept = default;
};

struct GlobalParameters {                 // global host parameters (§3.2, §3.3, §12); used by M16 and M17
    bool thru = false;                                                      // §3.3, §20
    bool consume = true;                                                    // §3.3, §20
    int chordInputChannel = constants::lanes::defaultChordInputChannel;     // 1..16, §3.2
    friend constexpr bool operator==(const GlobalParameters&, const GlobalParameters&) noexcept = default;
};

// Non-parameter lane data the audio thread needs (§12, §13). Lane color is NOT here (UI/state only).
struct LanePattern {
    StepPattern steps;
    TextGrid appliedTextGrid;            // last valid grid (§5.3); entryCount 0 = none (D-9)
    // Revisioned discard hand-off (D-32): document revision of the last edit of each step slot
    // (PatternDocument::revision() at the time of the edit; 0 = never edited since load/default).
    std::array<std::uint64_t, constants::lanes::maximumStepCount> stepEditRevision{};
    // Highest core discard revision of this lane that the document has applied (see M14, D-32).
    std::uint32_t acknowledgedDiscardRevision = 0;
    friend bool operator==(const LanePattern&, const LanePattern&) noexcept = default;
};

[[nodiscard]] LaneParameters clampLaneParameters(const LaneParameters&) noexcept;                // [RT] §17
// Grid of the ACTIVE timing source: base -> makeUniformGridLayout(baseTimingNoteLength(..), stepCount);
// textGrid -> makeGridLayout(appliedTextGrid.activeEntries()). Data-driven via a 2-entry function table.
[[nodiscard]] GridLayout makeActiveGridLayout(const LaneParameters&, const TextGrid& appliedTextGrid) noexcept; // [RT]
[[nodiscard]] int effectiveStepCount(const LaneParameters&, const TextGrid& appliedTextGrid) noexcept;          // [RT]
[[nodiscard]] SourceSelector makeSourceSelector(const LaneParameters&, int chordInputChannel) noexcept;        // [RT]
}
```

Change classes per field (§4.4, §4.6, §11), used by `LaneEngine::updateSettings`:

| Class | Fields | Takes effect |
|---|---|---|
| Immediate | `mute`, `latch` | at the block start where the new value is seen (D-21): mute on / latch on→off release lane voices and clear latch memory (§4.6) |
| Next step start — selector | `inputMode`, `followIndex`, `followChannel` | at the next step start (effective, i.e. swung, start, D-12) (§4.4, §11); release of the committed source still stops immediately |
| Next step start — grid (D-10) | `timingSource`, `baseTimingIndex`, `subdivision`, `stepCount`, `appliedTextGrid` | the derived `GridLayout` is compared; if different → pending; committed at the next step start with re-anchoring (§5.4, D-11, D-12) |
| At trigger (newly triggered notes) | `outputChannel`, `transpose`, `gatePercent`, `velocityMode`, step data | read when a step triggers (§11, §7) |
| At the preceding step start | `swingPercent` | read when the lane schedules the effective start of the following step, i.e. while processing the preceding step start (or at `startTransport`/`resynchronize`) — D-50 |
| Global, immediate | `chordInputChannel` | chord redefined at block start; lanes re-classify their source (D-24) |

### 4.2 M13 Block clock and lane position — `BlockClock.h`, `LanePosition.h/.cpp` (#10)

```cpp
namespace phu::arpeggiator {
struct BlockClock {                       // beat <-> sample mapping of the current block
    Beats blockStartBeat = 0;             // host playhead at sample 0
    Beats windowStartBeat = 0;            // first beat processed (D-16; == blockStartBeat unless continuous jitter)
    Beats windowEndBeat = 0;              // exclusive: blockStartBeat + numSamples / samplesPerBeat
    double samplesPerBeat = 0;
    int numSamples = 0;
    [[nodiscard]] SampleOffset toSampleOffset(Beats beat) const noexcept;   // beatToSampleOffset(beat, blockStartBeat, ...)
    [[nodiscard]] Beats toBeat(SampleOffset offset) const noexcept;         // blockStartBeat + offset / samplesPerBeat
    [[nodiscard]] bool contains(Beats beat) const noexcept;                 // windowStartBeat <= beat < windowEndBeat
};

struct LanePosition {
    std::int64_t loopIndex;   // floor((playhead - phaseOffset) / loopLength)
    int stepIndex;            // step containing the playhead
    bool isAtStepStart;       // |playhead - start(stepIndex)| <= beatEpsilon
};
// §10: position = (playhead - phaseOffset) mod loopLength. Precondition: !layout.isEmpty().
[[nodiscard]] LanePosition derivePosition(Beats playheadBeat, Beats phaseOffsetBeat, const GridLayout&) noexcept;   // [RT]
// Absolute beat of a step start: phaseOffset + (loopIndex*loopLengthTicks + stepStartTicks[stepIndex]) / ticksPerBeat
[[nodiscard]] Beats stepStartBeat(Beats phaseOffsetBeat, std::int64_t loopIndex, int stepIndex, const GridLayout&) noexcept;
// §5.4 re-anchoring (D-11): returns phaseOffset such that step `newStepIndex` of newLayout (loopIndex 0) starts at commitBeat.
[[nodiscard]] Beats reanchorPhaseOffset(Beats commitBeat, int newStepIndex, const GridLayout& newLayout) noexcept;
}
```

### 4.3 M14 Lane engine — `LaneEngine.h/.cpp` (#10)

One lane: commits pending changes at step boundaries, triggers notes, reacts to source changes. It owns no voices; it starts/ends them through `VoiceRegistry` with its `OwnerId` (= lane index).

```cpp
namespace phu::arpeggiator {

struct LaneContext {                       // per-call services, all non-owning
    VoiceRegistry& voices;
    OutputEventBuffer& output;
    const HeldNoteTracker& heldNotes;
    const BlockClock& clock;
    int chordInputChannel;                 // current global value (D-24)
};

struct LaneStatus {                        // published to the editor (M19 PlaybackStatus)
    int currentStepIndex = -1;             // step whose (effective) start was last passed; -1 = not running / no grid
    int activeStepCount = 0;               // committed grid
    bool isRunning = false;
    bool hasLatchedNote = false;
};

class LaneEngine {
public:
    explicit LaneEngine(OwnerId owner) noexcept;           // owner = lane index 0..7
    void reset() noexcept;                                  // §20 defaults, stopped, no latch memory, nothing pending

    // Once per block, AFTER transport_.update() built the BlockClock/LaneContext and BEFORE transport transitions
    // (§5.2 step 3 of this doc). `pattern` must stay valid until the next call (SequencerCore owns it; read-only).
    // Applies the change classes of 4.1 at clock.windowStartBeat / offset 0. Never resynchronizes itself: a
    // required resync (D-9: committed layout empty -> non-empty) is only recorded (resynchronizeRequested()).
    void updateSettings(const LaneParameters&, const LanePattern& pattern, LaneContext&) noexcept;
    [[nodiscard]] bool resynchronizeRequested() const noexcept;   // cleared by startTransport/stop/resynchronize

    void anchorTransportStart(Beats playheadBeat) noexcept; // phaseOffset = playhead, loopIndex 0, step 0 (§10); not running
    // anchor + running; the first step start is step 0 exactly at the playhead (never preceded by a swung step).
    void startTransport(Beats playheadBeat) noexcept;
    // Transport stop / bypass enter (§10, §21): lane voices are released by the caller (releaseAll, D-17);
    // clear latch memory, commit all pending changes WITHOUT re-anchoring (D-18), running = false. Phase offset kept.
    void stop(LaneContext&) noexcept;
    // Discontinuity (§10) / bypass exit (§21) / grid becoming non-empty (D-9): if releaseVoices, release lane
    // voices at `offset`; commit pending WITHOUT re-anchoring (D-19); derive position from playheadBeat;
    // next step start = the first step (from the grid step containing the playhead onward) whose EFFECTIVE
    // (swung) start is >= playheadBeat - beatEpsilon (D-13, D-4); running = true.
    void resynchronize(Beats playheadBeat, SampleOffset offset, bool releaseVoices, LaneContext&) noexcept;

    // After each input event that changed held notes (and after a chord-channel change), at that event's offset.
    void onHeldNotesChanged(SampleOffset offset, LaneContext&) noexcept;

    [[nodiscard]] Beats nextTimelineBeat() const noexcept;  // effective start of the next step; +inf if none
    void processNextTimelineEvent(LaneContext&) noexcept;   // exactly one step start at nextTimelineBeat()
    [[nodiscard]] LaneStatus status() const noexcept;

    // Revisioned step-tail discard (D-32). Called by SequencerCore::setPatterns with the new pattern: if
    // pattern.acknowledgedDiscardRevision >= discardRevision(), the mask is cleared.
    void onPatternAdopted(const LanePattern& pattern, std::uint64_t documentRevision) noexcept;
    [[nodiscard]] std::uint32_t discardRevision() const noexcept;     // number of committed decreases so far
    // Per slot: document revision of the snapshot the core held when the slot was discarded; 0 = not masked.
    [[nodiscard]] const std::array<std::uint64_t, constants::lanes::maximumStepCount>& discardMask() const noexcept;

private:
    OwnerId owner_;
    const LanePattern* pattern_ = nullptr;
    std::uint64_t currentDocumentRevision_ = 0;  // revision of the snapshot the core holds (set by SequencerCore)
    std::uint32_t discardRevision_ = 0;
    std::array<std::uint64_t, constants::lanes::maximumStepCount> discardedAtRevision_{};
    LaneParameters requested_{};                 // latest parameters
    SourceSelector committedSelector_{};
    GridLayout committedLayout_{}, requestedLayout_{};
    bool selectorPending_ = false, layoutPending_ = false, resynchronizeRequested_ = false;
    bool running_ = false;
    Beats phaseOffsetBeat_ = 0;
    struct NextStepStart { Beats effectiveBeat; std::int64_t loopIndex; int stepIndex; };
    NextStepStart nextStepStart_{};              // effective (swung) start, computed with the swing read at scheduling
    int currentStepIndex_ = -1;
    std::optional<SourceNote> lastObservedSource_;
    std::optional<SourceNote> latchMemory_;

    // Implementation-detail private helpers (not part of this sketch, no effect on the public
    // interface or persisted state): effectiveCommittedSelector folds the always-immediate (D-24)
    // chordInputChannel into committedSelector_ for a selectSource call; commitPendingSelector /
    // commitPendingLayoutWithoutReanchor commit a pending change without the step-start re-anchor
    // (used by stop/resynchronize/updateSettings-while-stopped); recordDiscardIfDecrease applies
    // the D-32 bookkeeping shared by every commit path; triggerStep/scheduleNextStepStart implement
    // behavior items 2 and 1.4 respectively.
};
}
```

Behavior (normative). Notation: `g_k` = unswung grid start of step *k*, `e_k` = effective start = `g_k − swingAdvanceBeats(len(k), k, swing, layout.swingApplies)` (D-4); the lane has **one** timeline event per step, its *step start* at `e_k` (D-12).

1. **Step start** of step *k* at `T = e_k` (`nextStepStart_`):
   1. If `layoutPending_`: let `k' = k mod newLayout.stepCount`; `phaseOffset = reanchorPhaseOffset(T, k', newLayout)` (grid start of k' := T); commit layout; k := k' (§5.4, D-11). The re-anchored step starts at `T` and is not swung again. No step skipped or repeated. Every layout commit (here, in `stop`, `resynchronize`, or while stopped) with both layouts non-empty and `new.stepCount < old.stepCount` records a discard: `++discardRevision_`, `discardedAtRevision_[s] = currentDocumentRevision_` for all `s ≥ new.stepCount` (§5.4 "discarded", D-32). If the new layout is empty: release lane voices at `T`, no further step starts (D-9).
   2. If `selectorPending_`: commit selector; re-evaluate source; if it classifies as `released` and latch is off → release lane voices at `T` (D-23).
   3. `currentStepIndex_ = k`; **trigger** (item 2) at `T`.
   4. Schedule the next step start: step k+1 (wrap → step 0 of loopIndex+1, never swung); `swing = requested_.swingPercent` **read now** (D-50); `nextStepStart_.effectiveBeat = g_{k+1} − swingAdvanceBeats(len(k+1), k+1, swing, committedLayout_.swingApplies)`. Because the advance is ≤ len/2, `e_{k+1} > T`; it may lie in the current or a future block.
2. **Trigger** of step *k* at `T` (§4.4, §6, §7, §8):
   1. If muted → nothing.
   2. `note = selectSource(heldNotes, committedSelector_)`; if none and latch on → `latchMemory_`; if none → nothing (position still advances, §4.5).
   3. If latch on → `latchMemory_ = note`.
   4. Step data = `pattern_->steps.step(k)`, or `defaultStepData()` if masked: `discardedAtRevision_[k] != 0 && pattern_->stepEditRevision[k] <= discardedAtRevision_[k]` (D-32). `k` is always < the committed step count, so slots beyond it are never read (I-1). If `!enabled` → nothing.
   5. `velocity = computeOutputVelocity(requested_.velocityMode, value[mode], note.velocity)`; none → nothing (§7.2).
   6. `pitch = note.pitch + transpose`; outside 0..127 → nothing (§8, no clamping).
   7. `voices.startVoice({owner_, outputChannel, pitch, velocity, T + gatedLengthBeats(len(k), gate)}, clock.toSampleOffset(T), output)`.
3. **Source change** (`onHeldNotesChanged`): `current = selectSource(...)`; `change = classifySourceChange(lastObservedSource_, current)`; store. If not running → return. If latch on, not muted and `current` → `latchMemory_ = current` (D-22). If `change == released` and latch off → `releaseOwner(owner_, offset)` (§4.4, §9.2) — immediately, also if the release falls between a swung step start `e_k` and its grid start `g_k` (the swung note belongs to step *k* and is released like any sounding lane note). `pitchChanged`/`started` → nothing now; adopted at the next step start (§4.2, §4.4, D-13); a source appearing after `e_k` (even before `g_k`) waits for step k+1.
4. **updateSettings** (at block start, offset 0, after the clock is known): mute off→on: release lane voices, clear latch memory. latch on→off: release lane voices, clear latch memory (§4.6). latch off→on: `latchMemory_ = current source` if any. Selector differs from committed → pending (if not running: commit immediately, D-18). Requested layout differs from committed → pending (if not running: commit immediately; if running and committed layout is empty: commit and set `resynchronizeRequested_`, executed by the core after the transport transition, D-9, §5.2 step 4).
5. Muted lanes keep processing step starts (position runs, pending changes commit, §4.5).
6. **Transport start / loop restart / swing**: `startTransport(p)` sets `nextStepStart_ = {p, 0, 0}`; nothing before step 0 is ever scheduled, so no swung step precedes the playhead at transport start. Loop restart is step 0 of the next loop at its unswung grid start (D-4). Pending changes take effect at the next *effective* step start, so a change requested after `e_k` but before `g_k` waits for step k+1.

Note-Off guarantee (§9.3): lane voices always carry a finite scheduled end in the registry; grid/step-count/timing-source changes never drop them (they end at their scheduled beat); stop/mute/latch-end/discontinuity/bypass release them explicitly.

### 4.4 M15 Transport tracker — `TransportTracker.h/.cpp` (#11)

```cpp
namespace phu::arpeggiator {
struct TransportInfo {                    // filled by the adapter from the host playhead (M19)
    bool isPlaying = false;
    bool hasMusicalPosition = false;      // host delivered PPQ position
    Beats playheadBeat = 0;               // PPQ at sample 0 of the block
    double tempoBeatsPerMinute = 0;       // <= 0 or missing -> treated as not playing (D-20)
};
enum class TransportTransition : std::uint8_t {
    idle,           // not playing, was not playing
    started,        // playing now, was not (or first playing block after reset, D-20)
    stopped,        // was playing, now not (incl. position/tempo became unavailable, D-20)
    continuous,     // playing, playhead continues (within tolerance, D-16)
    discontinuity   // playing, backward move or forward jump > one buffer (§10)
};
struct TransportUpdate { TransportTransition transition; BlockClock clock; };

class TransportTracker {
public:
    void reset() noexcept;    // forget history -> next playing block reports `started`
    // isPlaying := info.isPlaying && info.hasMusicalPosition && tempo > 0 && sampleRate > 0 (D-20).
    // clock: blockStart = playhead; windowEnd = playhead + numSamples/spb.
    // continuous: windowStart = expectedNextBeat (previous windowEnd); else windowStart = blockStart.
    // Discontinuity if blockStart < expected - tol  or  blockStart > expected + (windowEnd - blockStart) + tol,
    // tol = playheadJitterToleranceSamples / samplesPerBeat.
    // advanceExpectation = false (same-offset piece, D-51): the expectation is set to clock.windowStartBeat instead
    // of windowEndBeat, so the next call with the same playhead is `continuous` with the same window start.
    [[nodiscard]] TransportUpdate update(const TransportInfo&, double sampleRate, int numSamples,
                                         bool advanceExpectation = true) noexcept; // [RT]
private:
    bool wasPlaying_ = false;
    bool hasHistory_ = false;
    Beats expectedNextBeat_ = 0;
};
}
```

### 4.5 M16 Pass-through filter — `PassThroughFilter.h/.cpp` (#11)

```cpp
namespace phu::arpeggiator {
struct PassThroughPolicy {
    bool thru = false;
    bool consume = true;
    std::bitset<constants::midi::channelCount> followedChannels;     // bit (channel-1)
    // §3.3: thru && !(consume && followed(channel))
    [[nodiscard]] bool passesNoteOn(int channel) const noexcept;
};
// followed = chordInputChannel ∪ { followChannel of every lane whose CURRENT inputMode == channel } (D-25)
[[nodiscard]] PassThroughPolicy makePassThroughPolicy(const GlobalParameters&,
                                                      std::span<const LaneParameters, constants::lanes::laneCount>) noexcept;

class PassThroughFilter {
public:
    void reset() noexcept;
    // `event` already normalized (normalizeNoteEvent). Note-On: if policy passes -> voices.startVoice(
    // {passThroughOwner, ch, pitch, vel, noScheduledEnd}) and remember the handle per input key (ch,pitch),
    // replacing any previous entry (whose voice the registry retriggered). Note-Off: passed iff a handle is
    // remembered for the key (§3.3, independent of current Thru/Consume) -> voices.endVoice(handle) (may be
    // suppressed after ownership transfer, §9.1); entry cleared. CC64: nothing (interpreted by HeldNoteTracker
    // only; the raw CC64 is copied to the output by the adapter, D-31). The filter never outputs non-note events.
    void process(const MidiEvent& event, const PassThroughPolicy&, VoiceRegistry&, OutputEventBuffer&) noexcept; // [RT]
    // Bypass enter (§21), transport stop and discontinuity (§10, D-17): forget remembered handles (the caller
    // releases the voices via releaseAll). A later input Note-Off for such a key finds no handle and is suppressed.
    void clearTracking() noexcept;
    // During bypass (D-27): remember raw note state per key (Note-On sets, release clears).
    void trackRawNote(const MidiEvent& normalizedEvent) noexcept;
    // Bypass exit (D-27): adopt each raw-held key as a pass-through voice (no output) and remember its handle.
    void adoptRawNotes(VoiceRegistry&) noexcept;
private:
    std::array<std::array<VoiceHandle, constants::midi::pitchCount>, constants::midi::channelCount> passedVoiceByKey_{};
    std::array<std::bitset<constants::midi::pitchCount>, constants::midi::channelCount> rawHeld_{};
    std::array<std::array<std::uint8_t, constants::midi::pitchCount>, constants::midi::channelCount> rawVelocity_{};
};
inline constexpr OwnerId passThroughOwner = static_cast<OwnerId>(constants::lanes::laneCount);
}
```

### 4.6 M17 Block types, configuration, sequencer core — `BlockTypes.h`, `SequencerConfiguration.h/.cpp`, `SequencerCore.h/.cpp` (#11)

```cpp
// BlockTypes.h
namespace phu::arpeggiator {
// TransportInfo: see M15 (TransportTracker.h)
struct BlockInput {                       // one core call = one chunk of a host block (D-51)
    // Note events and CC64 only (D-31), sorted by sampleOffset (stable), offsets relative to the chunk start and
    // in [0, numSamples-1]; size <= inputEventsPerChunk.
    std::span<const MidiEvent> events;
    TransportInfo transport;              // playheadBeat = playhead at the chunk start
    double sampleRate = 0;                // Hz, > 0
    int numSamples = 0;                   // >= 0; 0 = no-op call (D-53), the adapter never sends it
    bool bypassed = false;                // host bypass (§21)
    // D-51: all events are at offset 0 and are a non-final piece of one oversized same-offset group; the core
    // processes block-start actions, voice ends at sample 0 and these events only (no lane step starts, no
    // later items) and does not advance its window; the next call repeats the same chunk start.
    bool moreInputAtSameOffset = false;
};
struct LaneDiscardState {                 // D-32 revisioned discard report, per lane
    std::uint32_t discardRevision = 0;    // LaneEngine::discardRevision()
    std::array<std::uint64_t, constants::lanes::maximumStepCount> discardedAtRevision{}; // 0 = not discarded
};
using StepDiscardState = std::array<LaneDiscardState, constants::lanes::laneCount>;   // audio -> message
struct BlockResult {                      // side information besides the events
    bool discardStateChanged = false;     // a lane recorded a discard or cleared its mask in this call
    std::uint32_t droppedOutputEvents = 0;            // this call (D-30)
};
// Output = OutputEventBuffer (M06), filled by processBlock.
}

// SequencerConfiguration.h
namespace phu::arpeggiator {
// GlobalParameters: see M12 (LaneConfiguration.h)
struct SequencerConfiguration {           // all host parameters, validated; built per block (M20)
    GlobalParameters global;
    std::array<LaneParameters, constants::lanes::laneCount> lanes;
    friend constexpr bool operator==(const SequencerConfiguration&, const SequencerConfiguration&) noexcept = default;
};
struct PatternSnapshot {                  // non-parameter audio data; trivially copyable
    std::array<LanePattern, constants::lanes::laneCount> lanes{};
    std::uint64_t documentRevision = 0;   // PatternDocument::revision() when the snapshot was made (D-32)
};
[[nodiscard]] SequencerConfiguration defaultSequencerConfiguration() noexcept;      // §20
[[nodiscard]] SequencerConfiguration clampConfiguration(const SequencerConfiguration&) noexcept; // [RT] §17
}

// SequencerCore.h — top-level composition
namespace phu::arpeggiator {
struct PlaybackStatus {                   // audio -> editor (M18 TripleBuffer); trivially copyable
    std::array<LaneStatus, constants::lanes::laneCount> lanes{};
    bool transportPlaying = false;
    bool bypassed = false;
    std::uint32_t droppedOutputEventsTotal = 0;
    std::uint32_t hostOutputGrowthCount = 0;      // set by the adapter (D-54, A-43)
};

class SequencerCore {
public:
    SequencerCore() noexcept;                       // §20 state; all members fixed-size (no heap)
    // [prepare, audio stopped] Only marks a pending reset; emits nothing. The next processBlock first releases
    // all sounding voices at offset 0 (D-44), then resets all state to §20/stopped (5.2 step 0); the first
    // playing block afterwards is a transport start (D-20).
    void reset() noexcept;
    // [RT] Adopt new pattern data (called by the adapter at the start of a host block, before the first chunk).
    // Ignores snapshots whose documentRevision is lower than the current one. Copies the snapshot verbatim into
    // the core-owned PatternSnapshot and calls LaneEngine::onPatternAdopted per lane (clears a lane's discard mask
    // once the document has acknowledged all its discards). It never writes step data: discarded slots are
    // masked at read time (D-32, I-1).
    void setPatterns(const PatternSnapshot&) noexcept;
    // [RT] One core call per chunk (§5.2 of this doc, D-51). `output` must have capacity >= outputEventsPerBlock;
    // it is cleared first. Returns side information.
    BlockResult processBlock(const BlockInput&, const SequencerConfiguration&, OutputEventBuffer& output) noexcept;
    [[nodiscard]] PlaybackStatus playbackStatus() const noexcept;   // [RT] after processBlock
    [[nodiscard]] StepDiscardState stepDiscardState() const noexcept; // [RT] D-32

private:
    SequencerConfiguration configuration_ = defaultSequencerConfiguration();
    PatternSnapshot patterns_{};
    HeldNoteTracker heldNotes_;
    VoiceRegistry voices_;
    PassThroughFilter passThrough_;
    TransportTracker transport_;
    std::array<LaneEngine, constants::lanes::laneCount> lanes_;   // constructed with OwnerId 0..7
    bool wasBypassed_ = false;
    bool flushPending_ = false;                                    // D-44
    bool transportPlaying_ = false;        // implementation addition (#11): last-seen transport
                                            // play state, so playbackStatus() can report it after
                                            // processBlock returns without re-deriving it from the
                                            // TransportTracker's internal (non-public) state.
    std::uint32_t droppedTotal_ = 0;
};
}
```

`SequencerCore` sets `voices_.setVoiceLimit(lane, maximumVoicesPerLane)` for each lane and unlimited for `passThroughOwner` (§9.4 limits lanes only). Size: ≈ 140 KB, held by value inside the processor (no heap per block). The adapter's chunking (M19, D-51) guarantees `events.size() <= inputEventsPerChunk` and `numSamples <= samplesPerChunk`.

### 4.7 M20 (core part) Parameter snapshot → configuration — `ParameterSnapshot.h`, `ConfigurationBuilder.h/.cpp` (#13)

The JUCE-free half of the parameter path, so it is unit-testable without JUCE (§18).

```cpp
namespace phu::arpeggiator {
// Raw values exactly as read from the host parameters (one atomic load each), before validation.
// Bools as 0/1 floats are converted by the reader; choices are 0-based indices; ints are rounded integers.
struct RawLaneParameterValues {
    int mute, latch, inputMode, followIndex, followChannel, outputChannel, transpose, gate,
        timingSource, baseTiming, subdivision, swing, stepCount, velocityMode;
};
struct ParameterSnapshot {
    int thru, consume, chordInputChannel;
    std::array<RawLaneParameterValues, constants::lanes::laneCount> lanes;
};
// Clamps every field to its constants range, maps indices to enums via the registries (§17 validation).
[[nodiscard]] SequencerConfiguration buildConfiguration(const ParameterSnapshot&) noexcept;   // [RT]
}
```

### 4.8 M21 (core part) Pattern document — `PatternDocument.h/.cpp` (#14)

Message-thread model of all non-parameter state (§12, §13): step data, text-grid text, applied grid, lane color. Pure C++ so state logic is testable without JUCE.

```cpp
namespace phu::arpeggiator {
struct LaneColor { std::uint8_t red, green, blue;               // opaque RGB (§12)
                   friend constexpr bool operator==(const LaneColor&, const LaneColor&) noexcept = default; };
[[nodiscard]] constexpr LaneColor defaultLaneColor(int laneIndex) noexcept;   // constants::colors::defaultLanePalette

struct LaneDocument {
    LanePattern pattern;                 // steps + applied (last valid) grid
    std::string textGridText;            // exactly what the user typed (may be invalid, §5.3)
    LaneColor color;
};

class PatternDocument {                  // [MT] only; PatternStore (M21) serializes access
public:
    PatternDocument();                                           // §20 defaults, palette colors
    void resetToDefaults();
    [[nodiscard]] const LaneDocument& lane(int laneIndex) const; // index clamped
    void setStepEnabled(int laneIndex, int stepIndex, bool enabled);
    void setStepValue(int laneIndex, int stepIndex, VelocityMode, int value);  // clamped
    // Parses; text is always stored. If valid: appliedTextGrid = grid. Returns the parse result for display (§5.3).
    TextGridParseResult setTextGridText(int laneIndex, std::string text);
    void setLaneColor(int laneIndex, LaneColor);
    // D-32: applies the core's discard report. For each lane with state.discardRevision > acknowledged revision:
    // every slot s with discardedAtRevision[s] != 0 and stepEditRevision[s] <= discardedAtRevision[s] is reset to
    // defaultStepData() (edits made after the snapshot the core had seen survive); then
    // acknowledgedDiscardRevision = state.discardRevision. Idempotent; bumps revision() if anything changed.
    void applyDiscardState(const StepDiscardState&);
    // Used by state loading (M22): replace a lane's data wholesale; the applied grid is re-derived by parsing
    // the stored applied text (D-9). Keeps acknowledgedDiscardRevision of the lane (core counters are not reset).
    void setLane(int laneIndex, LaneDocument);
    // Monotonic; every mutator increments it and stamps stepEditRevision of each step slot it writes.
    [[nodiscard]] std::uint64_t revision() const noexcept;
    [[nodiscard]] PatternSnapshot makeSnapshot() const;          // 8 LanePatterns + documentRevision = revision()
private:
    std::array<LaneDocument, constants::lanes::laneCount> lanes_;
    std::uint64_t revision_ = 1;
};
}
```

### 4.9 M23 (core part) Display scale — `DisplayScale.h/.cpp` (#15)

Pure geometry for §14 (common time scale), testable without JUCE.

```cpp
namespace phu::arpeggiator {
struct LaneDisplayGeometry {
    double loopWidthPixels = 0;                                         // loopLengthBeats * pixelsPerBeat
    std::array<double, constants::lanes::maximumStepCount + 1> stepStartPixels{}; // [stepCount] = loop end
    int stepCount = 0;                                                  // 0: empty grid -> "no grid" placeholder
};
struct DisplayScale {
    double pixelsPerBeat = 0;
    double contentWidthPixels = 0;     // longest loop * pixelsPerBeat; > available -> horizontal scrolling
    std::array<LaneDisplayGeometry, constants::lanes::laneCount> lanes{};
};
// pixelsPerBeat = availableWidth / longestLoopBeats, then raised to minimumStepDisplayWidth / shortestStepBeats
// if any step would be narrower than constants::editor::minimumStepDisplayWidth (§14). Swing ignored (§14).
// All lanes empty -> pixelsPerBeat 0, all widths 0.
[[nodiscard]] DisplayScale computeDisplayScale(std::span<const GridLayout, constants::lanes::laneCount> layouts,
                                               double availableWidthPixels) noexcept;
}
```

The editor computes layouts from the *requested* configuration (parameters + document), i.e. it shows the active timing source immediately; the playback indicator (`LaneStatus::currentStepIndex`) refers to the committed grid and is drawn only if `currentStepIndex < stepCount` of the displayed layout (while a grid change is pending the indicator refers to the committed grid; it is hidden if out of range).

---

