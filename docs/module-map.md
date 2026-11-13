# Module Map — Quick Reference for phu-sequential Developers

This is a compact token-efficient lookup table for agents working on specific modules. To read the full architecture and spec details:

1. Find the architecture section using `grep -n '### 4.3' docs/architecture.md` or similar (adjust section number), then view that range in the document
2. Find spec sections using `grep -n '^## 4' spec/arp-spec.md` (adjust § number per the module's citation)
3. Find design decisions by grepping the module's architecture section for `D-NN` markers (e.g., search section 4.3 for `D-12`)

| Module | Architecture section | Source files (repo status) | Test file(s) | Spec § | Key decisions | Depends on |
|--------|----------------------|----------------------------|--------------|--------|---------------|-----------|
| M01 Constants | 3.1 | `lib/sequencing/Constants.h` | ConstantsTests | §1, §12, §16.3 | D-1, D-3, D-5, D-7, D-30, D-33, D-36, D-37 | none |
| M02 MusicalTime / NoteValues | 3.2 | `lib/sequencing/MusicalTime.h`, `NoteValues.h` | MusicalTimeTests, NoteValuesTests | §5.1, §16.2 | D-3 | M01 |
| M03 StepTiming | 3.3 | `lib/sequencing/StepTiming.h`, `.cpp` | StepTimingTests | §5.1, §5.2, §5.4, §6 | D-4, D-10, D-11, D-12 | M02 |
| M04 SampleConversion | 3.4 | `lib/sequencing/SampleConversion.h` | SampleConversionTests | §10, §16.2, §17 | D-15 | M02 |
| M05 TextGridParser | 3.5 | `lib/sequencing/TextGridParser.h`, `.cpp` | TextGridParserTests | §5.2, §5.3 | D-8, D-9 | M03 |
| M06 MidiEvent / OutputEventBuffer | 3.6 | `lib/sequencing/MidiEvent.h`, `OutputEventBuffer.h`, `.cpp` | MidiEventTests, OutputEventBufferTests | §3.1, §7, §9.1, §9.3 | D-26, D-30, D-31 | M04 |
| M07 HeldNoteTracker | 3.7 | `lib/sequencing/HeldNoteTracker.h`, `.cpp` | HeldNoteTrackerTests | §3.1, §3.2, §4.3, §10 | none | M06 |
| M08 SourceSelection (strategies) | 3.8 | `lib/sequencing/sources/SourceSelection.h`, `.cpp`, `ChordFollowSource.h`, `.cpp`, `ChannelSource.h`, `.cpp`, `InputModeRegistry.h` | SourceSelectionTests | §4.2, §4.3, §16.1, §16.4 | none | M07 |
| M09 VelocityModes (strategies) | 3.9 | `lib/sequencing/velocity/VelocityMode.h`, `IncomingVelocity.h`, `ByStepVelocity.h`, `DeltaByStepVelocity.h`, `RelativeVelocity.h`, `VelocityModeRegistry.h`, `.cpp` | VelocityModesTests | §7.1, §7.2 | D-6 | M01 |
| M10 StepData | 3.10 | `lib/sequencing/StepData.h`, `.cpp` | StepDataTests | §5.2, §5.4, §7 | none (I-1 invariant) | M09 |
| M11 VoiceRegistry | 3.11 | `lib/sequencing/VoiceRegistry.h`, `.cpp` | VoiceRegistryTests | §9.1, §9.3, §9.4 | none | M06 |
| M12 LaneConfiguration | 4.1 | `lib/arpeggiator/LaneConfiguration.h`, `.cpp` | LaneConfigurationTests | §3.2, §4.2, §5.1, §5.2, §5.4, §12, §20 | D-9, D-10, D-12, D-21, D-32 | M03, M05, M08, M10 |
| M13 BlockClock / LanePosition | 4.2 | `lib/arpeggiator/BlockClock.h`, `LanePosition.h`, `.cpp` | LanePositionTests | §10, §16.2, §17 | D-4, D-11, D-16 | M03, M04 |
| M14 LaneEngine | 4.3 | `lib/arpeggiator/LaneEngine.h`, `.cpp` | LaneEngineTests | §4.2, §4.3, §4.4, §4.6, §6, §7, §8, §11 | D-12, D-19, D-20, D-22, D-23, D-24, D-25, D-32 | M12, M13, M11, M09, M08 |
| M15 TransportTracker | 4.4 | `lib/arpeggiator/TransportTracker.h`, `.cpp` | TransportTrackerTests | §10, §11, §21 | D-13, D-14, D-17, D-18, D-19, D-27 | M13 |
| M16 PassThroughFilter | 4.5 | `lib/arpeggiator/PassThroughFilter.h`, `.cpp` | PassThroughFilterTests | §3.3, §8, §9.3 | none | M11, M12 |
| M17 BlockTypes / SequencerConfiguration / SequencerCore | 4.6 | `lib/arpeggiator/BlockTypes.h`, `SequencerConfiguration.h`, `.cpp`, `SequencerCore.h`, `.cpp` | SequencerCoreTests | §3.1–§11, §16.1–§16.5, §17, §21 | D-1, D-2, D-14, D-16, D-17, D-24, D-27, D-28, D-30, D-31, D-32, D-38–D-43, D-44, D-50, D-51, D-52, D-53 | M14, M15, M16, M07 |
| M18 TripleBuffer | 3.12 | `lib/sequencing/TripleBuffer.h` (planned) | (no dedicated test yet) | §17 | none | (none; template utility) |
| M19 PluginProcessor / MidiConversion / HostTransport | 6.1 | `src/PluginProcessor.h`, `.cpp`, `src/MidiConversion.h`, `.cpp`, `src/HostTransport.h`, `.cpp`, `src/PlaybackStatus.h` | (adapter tests not yet in repo) | §2, §12, §13, §17, §21 | D-27, D-28, D-31, D-44, D-45, D-51, D-52, D-53, D-54 (see also A-43, A-44) | M17, M18 |
| M20 Parameters | 6.2 (+ 4.7 core) | `src/ParameterIds.h`, `.cpp`, `ParameterLayout.h`, `.cpp`, `ParameterReader.h`, `.cpp` + core: `lib/arpeggiator/ParameterSnapshot.h`, `ConfigurationBuilder.h`, `.cpp` | (APVTS attachments not yet tested; core builder tested via M17) | §12, §13, §16.4, §20 | D-28, D-33, D-36, D-46, D-52 | M20 core part + M17 |
| M21 PatternStore / PatternDocument | 6.3 (+ 4.8 core) | `src/PatternStore.h`, `.cpp` + core: `lib/arpeggiator/PatternDocument.h`, `.cpp` | (PatternStore integration tests pending; document logic testable via M22) | §5.2, §5.3, §12, §13, §14 | D-9, D-32, D-34 | M21 core + M18 |
| M22 State / Presets / Versioning | 6.4 | `src/StateSerializer.h`, `.cpp`, `StateMigration.h`, `.cpp`, `PresetManager.h`, `.cpp` | (state serialization tests pending; core serialization format testable via M22) | §13 | D-28, D-34, D-35, D-47, D-52 | M20, M21 |
| M23 Editor | 6.5 (+ 4.9 core) | `src/PluginEditor.h`, `.cpp` + core: `lib/arpeggiator/DisplayScale.h`, `.cpp` | (editor is JUCE-dependent, visual verification only) | §14, §15 | D-9, D-36, D-37, D-46, D-47 | M20, M21, M22, M23 core + M05 |
| M24 Factory presets | 6.6 | `presets/factory/*.phuseq` (planned; conditional build) | (embedded via binary data; loaded via M22) | §13, §20 | D-35 | M22 |

## Reading Recipe

1. **Identify your module**: Locate M01..M24 in the leftmost column.
2. **Find the spec sections**: The "Spec §" column cites section numbers. Run `grep -n '^## 3\|^## 4\|^## 5' spec/arp-spec.md` to find them.
3. **Read the architecture section**: The "Architecture section" column gives a reference like "4.3". Run `grep -n '^### 4.3' docs/architecture.md` and view that range.
4. **Look up key decisions**: Each decision D-NN is cited in the architecture section. Search the module section (e.g., 4.3) for "D-NN" to find the rationale.
5. **Check dependencies**: The rightmost column lists M-numbers your module depends on. Read those modules first if you're new to the codebase, or revisit them if behavior is unclear.

**Notes:**
- Rows M18–M24 list files as designed in the architecture; only `src/PluginProcessor.*`, `src/PluginEditor.*` and `lib/arpeggiator/DisplayScale.*` exist today. Verify with `glob` before relying on a path.
- *Planned* files (TripleBuffer, adapter tests, factory presets) are not yet in the repo; see architecture for their design.
- Modules are stably numbered M01..M24 per docs/architecture.md §1.1 and used by test-matrix.md.
- All file paths are relative to the repo root; they exist unless marked "planned".
- Spec citations like "§5.1" refer to top-level sections (## heading); architecture citations like "D-12" refer to rationales within module sections (### heading).
