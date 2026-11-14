# phu-sequential — Architecture — 09-issue-to-module-map.md

## 9. Issue → module mapping

| Issue | Implements | Files | Sections here |
|---|---|---|---|
| #2 | this document | `docs/architecture.md` | all |
| #3 | test matrix (spec rule → scenario → owning module M01–M24, tags `[REQ-x.y-n]`) | `docs/test-matrix.md` | §1.2, §11, §12 |
| #4 | build system, plugin target skeleton, CI, empty test target | root `CMakeLists.txt`, `CMakePresets.json`, `cmake/*`, `lib/CMakeLists.txt`, `src/CMakeLists.txt`, `tests/CMakeLists.txt`, `.github/workflows/*`, `scripts/*` | §8 |
| #5 | M01, M02, M03, M04 | `lib/sequencing/Constants.h`, `MusicalTime.h`, `NoteValues.h`, `StepTiming.*`, `SampleConversion.h` | §3.1–§3.4 |
| #6 | M05 | `lib/sequencing/TextGridParser.*` | §3.5 |
| #7 | M06 (`MidiEvent`), M07, M08 | `MidiEvent.h`, `HeldNoteTracker.*`, `sources/*` | §3.6–§3.8 |
| #8 | M09, M10 | `velocity/*`, `StepData.*` | §3.9, §3.10 |
| #9 | M06 (`OutputEventBuffer`), M11 | `OutputEventBuffer.*`, `VoiceRegistry.*` | §3.6, §3.11 |
| #10 | M12, M13, M14 | `lib/arpeggiator/LaneConfiguration.*`, `BlockClock.h`, `LanePosition.*`, `LaneEngine.*` | §4.1–§4.3 |
| #11 | M15, M16, M17, test support | `TransportTracker.*`, `PassThroughFilter.*`, `BlockTypes.h`, `SequencerConfiguration.*`, `SequencerCore.*`, `tests/support/*` | §4.4–§4.6, §5, §7.3 |
| #12 | M18, M19 (with temporary `defaultSequencerConfiguration()` until #13), M21 stub | `TripleBuffer.h`, `src/PluginProcessor.*`, `MidiConversion.*`, `HostTransport.*`, `PlaybackStatus.h` (re-export of the core type), stub `src/PatternStore.h/.cpp` (§6.3), `SequentialAdapterTests` target | §3.12, §6.1, §6.3, §7 |
| #13 | M20 (core + adapter) | `ParameterSnapshot.h`, `ConfigurationBuilder.*`, `src/ParameterIds.*`, `ParameterLayout.*`, `ParameterReader.*` | §4.7, §6.2 |
| #14 | M21 (core + adapter), M22 | `PatternDocument.*`, `src/PatternStore.*`, `StateSerializer.*`, `StateMigration.*`, `PresetManager.*` | §4.8, §6.3, §6.4 |
| #15 | M23 (core + adapter) | `DisplayScale.*`, `src/editor/*` | §4.9, §6.5 |
| #16 | M24 | `presets/factory/*.phuseq`, `presets/factory/README.md`, binary-data registration | §6.6 |

Notes: #12 creates `PatternStore` as a stub with exactly the API in §6.3 ("#12 stub vs #14": constructor publishing the default snapshot at revision 1, `audioSide()`, no-op `applyDiscardState`); it needs the core's `PatternSnapshot`/`StepDiscardState` types, which exist since #11. #14 keeps those signatures, replaces `PatternStore.cpp` and adds the mutators, `replaceDocument`, `copyDocument`, the mutex and the `ChangeBroadcaster` base. `PlaybackStatus` is defined in the core (M17); `src/PlaybackStatus.h` is an alias header for editor includes.

---

