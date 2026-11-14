# phu-sequential — Architecture — 10-design-decisions.md

## 10. Decisions index

All decisions are binding for implementation; those marked † resolve a spec ambiguity (§12) and are **provisional** until the spec owner confirms them.

| ID | Decision | Rationale / spec |
|---|---|---|
| D-1 | Three layers: `lib/sequencing` (reusable, `phu::sequencing`), `lib/arpeggiator` (plugin core, `phu::arpeggiator`), `src` (JUCE, `phu::plugin`) | §16.1, §16.5; reusable vs plugin-specific separation |
| D-2 | `SequentialCoreLib` is a STATIC library | size of core; template uses INTERFACE |
| D-3 | Positions in `double` beats; grid structure in integer ticks, 960/beat | §10, §17; exact triplet/dotted lengths, no drift |
| D-4 † | Swing *advances* odd 0-based (even 1-based) steps: they start `stepLen·swing/200` beats **earlier**, inside the previous step; step 0 (loop start) is never shifted; effective starts stay strictly increasing; gate measured from the effective start | §5.1 "backward" |
| D-5 † | Default step values: By Step 100, Delta 0, Relative 100 % | §5.4/§20 "default velocity values" unspecified |
| D-6 † | Relative velocity: `(v·p + 50) / 100` integer, i.e. round half up | §7.1 says "divided by 100" without rounding |
| D-7 † | Gate, swing, relative percentages are integers | §6, §5.1, §7.1 give integer ranges |
| D-8 † | Text-grid grammar: comma-separated, ASCII space/tab around entries, numerator exactly `1`, denominators exact tokens, lowercase `t` or `.`; empty/whitespace-only text invalid | §5.2, §5.3 |
| D-9 † | Default text grid text is empty → no applied grid; Text Grid source without applied grid = empty layout: lane silent, no position, no step-data discard; when a grid gets applied it commits immediately and the position is derived from the playhead with the existing phase offset; state stores typed text and applied grid separately | §5.2, §5.3, §20 silent |
| D-10 † | Changes of base timing, subdivision, step count, timing source and applied text grid are all "grid changes" (next step start + re-anchoring) | §5.4, §11 |
| D-11 † | Re-anchoring: at commit beat `T` with old next step `k`, `k' = k mod newCount`, `phaseOffset = T − newStart[k']`, loop index 0 | §5.4 |
| D-12 † | One timeline event per step at its *effective (swung) start* `e_k`: pending commits (selector, grid, re-anchor at `T = e_k`, step-count discards) first, then the trigger (sampling, note start). A change requested after `e_k` waits for step k+1. Transport start: step 0 exactly at the playhead, nothing scheduled before it; loop restart is the unswung step 0 | §5.1, §11 "next step start" |
| D-13 | A lane never triggers mid-step; a source appearing or changing mid-step is used from the next trigger | §4.4, §4.2 |
| D-14 † | Same-time ordering rules of §5.3 here | spec silent |
| D-15 | Sample offset = `floor((beat − blockStart)·spb + 1e-6)` clamped to `[0, n−1]` | §10, §16.2 |
| D-16 † | Continuity tolerance: ≤ 1 sample backward, ≤ block length forward → continuous (window starts at the expected beat); otherwise discontinuity | §10 "backward or forward beyond one buffer" |
| D-17 † | Transport stop / discontinuity release **all** voices (lane and pass-through) and clear pass-through tracking; later stale input Note-Offs for those keys are suppressed, not forwarded | §10 "all sounding notes", §9.3 |
| D-18 † | Pending changes are committed (without re-anchoring) when the transport stops; while stopped, changes apply immediately (open point of #2) | §11 silent for stopped transport |
| D-19 † | On discontinuity, pending changes are committed without re-anchoring, then the position is re-derived from the playhead | §10 |
| D-20 † | First playing block after reset is a transport start; missing PPQ, missing tempo or tempo ≤ 0 = not playing | §10 silent |
| D-21 | Parameters are read once per block and act from offset 0 of that block (core rules then apply) | §17; JUCE has no sample-accurate APVTS |
| D-22 † | Latch memory is updated at event time whenever the committed source delivers a note while running and unmuted; it survives selector changes; cleared on mute, latch off, stop, bypass | §4.6, §7.3 |
| D-23 † | A committed selector change that yields no source (latch off) releases the lane's voices at that step start | §4.4, §11 |
| D-24 † | Chord input channel change takes effect immediately (block start) for all lanes and for Consume | §3.2, §11 silent |
| D-25 † | Consume channel set = chord input channel ∪ follow channels of all lanes whose *current* input-mode parameter is Channel, muted lanes included | §3.3 |
| D-26 † | All generated Note-Offs (lanes and pass-through) carry release velocity 0; incoming Note-On velocity 0 is treated and emitted as Note-Off | §9.1, §3.1 |
| D-27 † | Bypass: held-note tracking and transport anchoring continue during bypass; raw notes held at bypass exit are adopted as pass-through voices (no new output) so their later Note-Offs pass | §21 |
| D-28 | Host bypass is the JUCE bypass parameter `bypass` plus `processBlockBypassed`; not stored in state/presets | §21, §13 |
| D-29 † | `startVoice` emits retrigger Note-Off, then voice-limit eviction Note-Off, then Note-On | §9.1, §9.4 |
| D-30 | Capacities: core input 4096 note/CC64 events per core call (adapter streams chunks, D-51; no per-host-block ceiling, input never dropped); core output 16384 events per call partitioned into 14 336 general slots and 2048 Note-Off-only slots (Note-Ons admitted only while 2048 slots stay free; §5.2 bound 13 376 ⇒ nothing refused); host output partition and residual drop cases per D-45 | §17, §9.3 |
| D-31 | Non-note events (other CCs, pitch bend, aftertouch, program change, SysEx, …) never enter the core: the adapter copies them byte-for-byte from the host input to the output at their original offsets (before core output at equal offsets). Thru/Consume apply to notes only; CC64 is additionally given to the core for sustain | §3.3 "always passed through unchanged" |
| D-32 | Pattern data: message-thread `PatternDocument` in `PatternStore` (mutex) → `TripleBuffer` → core copy, revisioned (`documentRevision`, per-slot `stepEditRevision`). On a committed step-count decrease the core records `discardedAtRevision` for the tail and masks those slots as defaults at read time until a snapshot acknowledges the lane's `discardRevision`; reported via `TripleBuffer<StepDiscardState>`; `PatternDocument::applyDiscardState` resets only slots not edited after the discard (invariant I-1) | §5.2, §5.4, §17 |
| D-33 † | Follow Channel default is 1 | §20 silent |
| D-34 | State/preset format: ValueTree XML `<PhuSequential formatVersion kind>`, registry keys for choices, all 64 steps stored, migration chain, fallback rules | §13 |
| D-35 | User presets: `*.phuseq` in `userApplicationDataDirectory/phu/phu-sequential/Presets`; factory presets in BinaryData; host programs unused | §13 |
| D-36 | Editor size: default 1280×820, min 1000×420, max 3840×2400, freely resizable (open point of #2) | §14 |
| D-37 | Minimum step display width 16 px; lane row height 140 px | §14 named constant |
| D-38 | CMake 3.31.6, JUCE 9.0.5, Catch2 v3.16.1, C++20, MSVC/GCC/AppleClang CI | §15 |
| D-39 † | VST3 category "Fx\|Tools" (no MIDI-effect category exists); AU MIDI processor on macOS as extra | §2 |
| D-40 † | A note whose start and gated end fall on the same sample is emitted Note-On then Note-Off at that offset | §6 |
| D-41 † | CC64 sustain is interpreted per channel on all 16 channels; non-note events are never consumed | §3.1, §3.3 |
| D-42 | phu-audio-lib (template dependency) is not used | no audio path |
| D-43 | Two test targets: `SequentialCoreTests` (no JUCE) and `SequentialAdapterTests`; allocation guard for RT tests | §18, §17 |
| D-44 † | After `prepareToPlay`/reset the first block releases all previously sounding voices at offset 0 | §9.3 |
| D-45 | Output: one processor-owned scratch `juce::MidiBuffer` reserved (`outputMidiReserveBytes`) in `prepareToPlay`, never swapped; merged output (raw + core) is written into it by `OutputMidiWriter` with a fixed byte partition: `noteOffReserveBytes` (2048·9) only for core Note-Offs, `rawReleaseReserveBytes` (48·9) for raw release events; core Note-Offs are never dropped; raw events and Note-Ons beyond ≈ 114 k events per host block can be dropped (counted, A-44); then copied into the host buffer (D-54) | §17, §9.3 |
| D-46 | Control strip shows Follow Index in Chord Follow mode and Follow Channel in Channel mode | UI sketches |
| D-47 † | A rejected preset (newer/corrupt) falls back to the default state like host state | §13 |
| D-48 † | Mute on (and latch on→off) releases the lane's sounding voices immediately at block start; mute keeps position running | §4.5, §4.6, §11 |
| D-49 † | Input key tracking is one entry per (channel, pitch): a repeated Note-On on a held key replaces it (pass-through: retrigger via registry), the first release ends it | §3.1, §3.3 |
| D-50 † | Swing is sampled once per step, when the lane schedules that step's effective start, i.e. while processing the preceding step start (or at `startTransport`/`resynchronize`); deterministic, block-size independent; a later swing change affects the step after next | §11 "swing … newly triggered notes", §5.1 |
| D-51 | Adapter input chunking: core-routed events streamed from the host buffer into a reused 4096-event array; chunks cut only between complete sample-offset groups (offsets in `[chunkStart, chunkEnd)`, ≤ `samplesPerChunk` samples); an oversized same-offset group is split in input order into pieces processed at the same offset with `moreInputAtSameOffset` | §9.3, §17 |
| D-52 | State application is atomic: `setStateInformation`/preset load builds one versioned `PendingState` {generation, parameters, patterns} handed to the audio thread via a triple buffer and applied at the next block start; APVTS/UI updates are marshalled to the message thread separately; the audio thread uses the snapshot parameters until APVTS has synced that generation; APVTS sync callbacks are generation-guarded (stale ones return early) and `parametersSyncedGeneration_` only advances by CAS-max | §13 |
| D-53 † | `numSamples == 0`: no core call, no output, no state change; pending Note-Offs, flushes and transitions happen in the next non-empty block; host events of consecutive zero-sample blocks accumulate in a fixed-capacity `DeferredInputStore` (nothing cleared until the next non-empty block) and are delivered at offset 0 of it; release-class events are never dropped (a newer release event subsumes an earlier one with the same identity; when full, the oldest non-release entry is evicted, counted) | §17, §9.3 |
| D-54 | Host copy: `midi.clear(); midi.addEvents(scratch)` (byte append) after all host input was read. If the host buffer's existing storage is too small, it is grown once to the scratch capacity on the audio thread and counted (`hostOutputGrowthCount`) instead of truncating (which could drop Note-Offs). Known deviation from §17 "no allocation on the audio thread", owner decision pending (A-43) | §17, §9.3 |

### 10.1 Open points required by #2

- **Pending changes on transport stop** → D-18: at the stop transition every lane releases its voices, clears latch memory and commits all pending selector and grid changes (step-tail discard included) *without* re-anchoring; the phase offset is irrelevant afterwards because the next transport start re-anchors at the start playhead (§10). While stopped, parameter changes are applied immediately at block start.
- **Editor size limits** → D-36/D-37: default 1280×820 (global bar + ~5.5 lanes visible), minimum 1000×420 (control strip + ≥ 2 lane rows + 16 px steps for a 16-step lane), maximum 3840×2400 (4K), aspect free; vertical scroll for lanes, shared horizontal scroll for steps.

---

