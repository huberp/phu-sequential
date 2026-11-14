# phu-sequential — Architecture — 11-testing-architecture.md

## 11. Testing architecture (§18)

- `SequentialCoreTests` (Catch2, no JUCE) — one file per module `tests/core/<Module>Tests.cpp`; scenario tests via `tests/support/ScenarioRunner`:
  ```cpp
  namespace phu::tests {
  struct ScenarioBlock { int numSamples; arpeggiator::TransportInfo transport; bool bypassed = false;
                         std::vector<sequencing::MidiEvent> events; std::optional<arpeggiator::SequencerConfiguration> configuration;
                         std::optional<arpeggiator::PatternSnapshot> patterns; };
  struct RecordedEvent { std::int64_t absoluteSample; sequencing::MidiEvent event; };
  class ScenarioRunner {     // owns SequencerCore + OutputEventBuffer; sample rate default 48000
  public:
      explicit ScenarioRunner(double sampleRate = 48000.0);
      void run(const ScenarioBlock&);                                  // calls setPatterns/processBlock
      // Convenience: playing blocks of fixed size at constant tempo from a start beat.
      void play(Beats startBeat, double bpm, int blockSize, int blockCount, std::span<const sequencing::MidiEvent> eventsAtAbsoluteSamples = {});
      [[nodiscard]] const std::vector<RecordedEvent>& output() const noexcept;
      [[nodiscard]] arpeggiator::PlaybackStatus status() const noexcept;
  };
  }
  ```
- Invariant checks run after every scenario: every Note-On has exactly one later Note-Off for the same (channel, pitch) by the end of a stop/bypass flush (§9.3); output offsets non-decreasing; `AllocationGuard` count 0 inside `processBlock`.
- Tags: `[REQ-<spec section>-<n>]` from `docs/test-matrix.md` (#3) plus module tag `[M14]`.
- Core scenario tests include: backward swing (examples of §3.3 M03, transport start at an arbitrary PPQ (incl. one where a swung step would fall) yields an immediate unswung step 0 at the playhead with no preceding swung trigger (D-4, §4.3 of this doc rule 6, §5.2 step 4.2), loop restart, pending change between `e_k` and `g_k`), swing sampling time (D-50), revisioned discard vs concurrent edit (D-32), stop/discontinuity releasing pass-through voices (D-17), same-offset pieces (D-51; incl. a due voice end at offset 0 while an earlier lane start is suppressed), `numSamples == 0` (D-53), core output partition (Note-Ons refused only while 2048 Note-Off slots would not stay free, D-30).
- `SequentialAdapterTests` (Catch2 + JUCE modules, no plugin wrapper): MIDI conversion (chunk cutting between offset groups, oversized same-offset group, > 16384 events per host block, raw non-note/SysEx copy and ordering, output overflow policy: generated Note-Offs never dropped when raw release input and raw/Note-On traffic fill the scratch, exact refusal thresholds 114 412 / 114 460, suppressed Note-Off after a refused Note-On; host copy: no `swapWith`, 0 allocations into a host buffer with sufficient storage, exactly 1 growth into a small one and 0 afterwards; consecutive zero-sample blocks: cumulative deferral, release subsumption, eviction of the oldest non-release entry, counts), atomic state application (D-52; out-of-order APVTS callbacks: a stale generation returns early, `parametersSyncedGeneration_` never decreases), parameter layout (IDs, ranges, defaults = §20), state round-trip/migration/fallback fixtures, factory presets load cleanly (skipped when `PHU_HAS_FACTORY_PRESETS=0`).
- pluginval in CI (§15) for the built plugin.

---

