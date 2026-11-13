---
applyTo: "**/*.cpp,**/*.h,**/*.hpp,**/*.cc,**/*.cxx"
---

# C++ Best Practices for phu-sequential

## Code Style & Structure

- **Header Guards**: Always use `#pragma once` for header files
- **Namespaces**: Use `phu::sequencing` for reusable core, `phu::arpeggiator` for plugin-specific core, `phu::plugin` for JUCE adapter
- **Naming**: Use `camelCase` for functions, `PascalCase` for types, `snake_case` for variables, `UPPER_CASE` for constants
- **Include Order**: Group includes by module (sequencing/, arpeggiator/, JUCE), with blank lines between groups
- **No using namespace**: Never use `using namespace` in headers; use qualified names or `using` declarations in .cpp files only
- **Headers**: Always include necessary headers explicitly. Don't assume they are reliably included from other headers. Assuming this might break multi-platform build.

## Type Safety & Modern C++

- **Strong Types**: Prefer `std::int64_t`, `std::uint8_t` over `int`/`unsigned` for specific sizes
- **constexpr**: Use `inline constexpr` for constants in `Constants.h` (single source of truth per §16.3)
- **noexcept**: Mark all [RT] functions `noexcept` (audio thread: no allocation, no locks, no blocking, no exceptions)
- **Bounds**: Validate all inputs; use `clampLaneParameters()` pattern for parameter validation
- **Optional**: Use `std::optional` for potentially absent values (e.g., `lastObservedSource_`)
- **Arrays**: Prefer `std::array<T, N>` over raw arrays with `inline constexpr` size

## Memory & Performance

- **No Allocation on [RT]**: Audio thread code must never allocate (use stacks, arenas, or pre-allocated buffers)
- **No Exceptions on [RT]**: Audio thread code must be `noexcept`
- **No Locks on [RT]**: Use lock-free structures like `TripleBuffer` for message↔audio communication
- **No Globals**: Avoid mutable global state; use dependency injection
- **No Singletons**: Avoid singletons with mutable state in core modules
- **No Randomness**: Core never uses random (spec has no random behavior; if added, use injected seeded generator)
- **Capacity Limits**: Respect `capacity::*` constants (e.g., `inputEventsPerChunk = 4096`)

## Error Handling

- **Assertions**: Use `static_assert` for compile-time checks in headers
- **Validation**: Return `std::expected` or use output parameters for fallible operations
- **Fail-Fast**: In debug builds, assert on invalid state; in release, clamp and continue

---

## JUCE Best Practices

### JUCE Usage Rules

- **No JUCE in lib/**: Nothing in `lib/sequencing/` or `lib/arpeggiator/` may include JUCE headers
- **JUCE in src/ Only**: JUCE usage is confined to `src/` directory (M19 plugin adapter layer)
- **Minimal JUCE Features**: Disable unnecessary JUCE modules in `cmake/juce_config.cmake`
- **JUCE Types**: Use JUCE types only at the boundary; convert to/from core types immediately
- **JUCE Threading**: Use `MessageManagerLock` for UI thread, never in audio thread

### Audio Processing

- **Block Processing**: Process in blocks; use `BlockClock` for beat↔sample conversion
- **MIDI Buffers**: Use `juce::MidiBuffer` at boundaries; convert to `OutputEventBuffer` for core
- **Sample Accuracy**: Use `SampleOffset` (int32) for sample positions, `Beats` (double) for musical time
- **Transport**: Read host transport in `HostTransport.h`; convert to core `TransportInfo`

### Editor & UI

- **LookAndFeel**: Use `EditorLookAndFeel` for consistent styling
- **Component Lifecycle**: Always check for null in `paint()` and event handlers
- **Repaint**: Use `repaint()` sparingly; batch UI updates
- **Async Updates**: Use `AsyncUpdater` or `Timer` for deferred UI work

### State Management

- **State Serialization**: Use `StateSerializer` for plugin state; version with `currentFormatVersion`
- **Preset Management**: Use `PresetManager` for preset loading/saving
- **Parameter Automation**: Use `ParameterLayout` and `ParameterReader` for automatable parameters

---

## Architecture-Specific Rules

### Module Boundaries

- **Dependency Order**: `sequencing` → `arpeggiator` → `plugin` (JUCE)
- **No Upward Dependencies**: `sequencing` depends on nothing but C++20 standard library
- **No Cross-Module State**: Modules communicate via explicit parameters, not shared state
- **One Unit Per File**: Each module (M01-M24) has its own .h/.cpp pair

### Real-Time Guarantees

- **RT Markers**: Functions marked [RT] in architecture.md must be `noexcept`, O(bounded), no allocation, no locks
- **MT Markers**: Functions marked [MT] are message/UI thread only (may allocate/lock)
- **Timing**: Use `ticksPerBeat = 960` for all tick calculations; `beatEpsilon = 1.0e-6` for comparisons
- **No Drift**: Compute absolute beat positions as `phaseOffset + (loopIndex * loopLengthTicks + stepStartTicks[k]) / ticksPerBeat` (never accumulated)

### Data Flow

- **Input**: MIDI events → `MidiConversion` → `HeldNoteTracker` → `SourceSelector`
- **Processing**: `SequencerCore::processBlock` → `LaneEngine` → `VoiceRegistry` → `OutputEventBuffer`
- **Output**: `OutputEventBuffer` → JUCE `MidiBuffer` → Host
- **Pattern Data**: Message thread edits → `TripleBuffer` → Audio thread read via `PatternStore`
- **Playback Status**: Audio thread → `TripleBuffer` → Message thread → Editor

### Testing

- **Unit Tests**: Use Catch2 v3; one test file per module (e.g., `StepTimingTests.cpp`)
- **Allocation Guards**: Use `AllocationGuard` in tests to verify no allocation on [RT] paths
- **Scenario Testing**: Use `ScenarioRunner` to drive `SequencerCore` with scripted blocks
- **Test Coverage**: Enable with `ENABLE_COVERAGE` flag in CMake

---

## Quality Guardrails

### Static Analysis

- **Compiler Warnings**: Enable with `ENABLE_STRICT_WARNINGS` (Wall, Wextra, Wpedantic on GCC/Clang; /W4 on MSVC)
- **Sanitizers**: Enable with `ENABLE_SANITIZERS` (AddressSanitizer, UndefinedBehaviorSanitizer)
- **Clang-Tidy**: Run with `clang-tidy` for modern C++ checks
- **Clang-Format**: Format code consistently (configure via `.clang-format`)

### Dynamic Analysis

- **Valgrind**: Run tests under valgrind to detect memory issues
- **Pluginval**: Validate VST3 plugin with `pluginval` (strictness level 5 in CI)

---

## Common Pitfalls & How to Avoid Them

1. **Floating Point Drift**: Never accumulate beat positions; always compute from scratch
2. **Sample Rounding**: Use `sampleRoundingEpsilon = 1.0e-6` for sample position comparisons
3. **Voice Management**: Always use `VoiceRegistry` for voice lifecycle; never manage voices manually
4. **Thread Safety**: Use `TripleBuffer` for message↔audio communication; never share mutable state
5. **Parameter Validation**: Always clamp parameters using `clampLaneParameters()` before use
6. **Grid Changes**: Re-anchor phase offset when grid changes to avoid skipped/repeated steps
7. **Swing Timing**: Read swing percent at step scheduling time (D-50), not at trigger time
8. **Latch Behavior**: Clear latch memory on mute, transport stop, bypass enter (D-17, D-18)
9. **Discard Revision**: Track discard revisions to prevent old step data from resurfacing (D-32)
10. **JUCE Threads**: Never call JUCE code from audio thread; use async message posting
11. **Catch2 string_view Linker Error**: When comparing `std::string_view` fields from library structures in Catch2 tests, explicitly cast to `std::string` to avoid LNK2019/LNK1120 linker errors on Windows. Catch2 v3's `StringMaker<std::string_view>` specialization may not be properly linked. **Fix**: Use `CHECK(static_cast<std::string>(obj.name) == "value")` instead of `CHECK(obj.name == "value")`. This applies to every assertion macro (`CHECK*`, `REQUIRE*`, `WARN`, `INFO`, `CAPTURE`), including `!=` and comparisons between two `string_view` fields. **Enforced** at three layers, all running `scripts/check_catch2_string_view.cmake` (pure CMake, `cmake -P`, no Python): (1) a `postToolUse` hook in `.github/hooks/catch2-string-view-guard.json` runs it after every agent tool call and reports violations back to the agent immediately; (2) a "Check Catch2 string_view usage" step runs before configure in every build workflow (`build-linux.yml`, `build-macos.yml`, `build-windows-vst3.yml`); (3) an `execute_process` check at configure time in `tests/CMakeLists.txt` that fails with `FATAL_ERROR` before any compile (not a CTest test, because CTest reported it as "Not Run" on macOS). Do not bypass or remove any layer; if the guard flags a false positive, fix the assertion or narrow the script's detection instead.
12. **Every Platform Must Build**: All code, including tests and scripts (CMake scripts, shell/Python helpers, CI steps), must work on every CI platform: Windows x64, Linux x64 and arm64, macOS x64 and arm64. Use only CMake-native or cross-platform mechanisms. Do not depend on a specific interpreter path or on tools that CI runners do not guarantee (for example, `find_package(Python3)` inside CTest failed on macOS runners). Before finishing a change, check the CI matrix in `.github/workflows/` and verify that any new script or test has no platform-specific dependency.
13. **Test Names Must Be ASCII**: Never use non-ASCII characters (e.g. the section sign `§`, `–`, `≥`) in `TEST_CASE`/`SECTION`/`SCENARIO`/`GIVEN`/`WHEN`/`THEN` names or tags. `catch_discover_tests` hands the name to the test executable on its command line; on Windows this goes through the ANSI code page, so the name no longer matches and CTest reports `***Failed` while Linux/macOS pass. The log shows the mojibake (`§` UTF-8 bytes rendered as `┬º`), which is why such names look cryptic. Write `sec. 9.4` instead of `§9.4`. Non-ASCII in comments is fine. **Enforced** by Pass 3 of `scripts/check_catch2_string_view.cmake` (same three layers as item 11).

---

## Quick Reference

| Concept | Location | Key File |
|---------|----------|----------|
| Constants | `lib/sequencing/Constants.h` | All `inline constexpr` |
| Voice Management | `lib/sequencing/VoiceRegistry.h` | `startVoice`, `endVoice` |
| Triple Buffer | `lib/sequencing/TripleBuffer.h` | `writeBuffer`, `publish`, `update` |
| Lane Engine | `lib/arpeggiator/LaneEngine.h` | `processNextTimelineEvent` |
| Sequencer Core | `lib/arpeggiator/SequencerCore.h` | `processBlock` |
| Plugin Processor | `src/PluginProcessor.h` | JUCE adapter |
