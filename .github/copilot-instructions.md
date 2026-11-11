# Copilot Instructions for phu-sequential

**When making changes to the plugin, always:**

1. **Consult `spec/arp-spec.md`** — the single source of truth for plugin behavior and architecture (§2, §15, §16, §18).
2. **Consult `docs/architecture.md`** — the specification for directory layout, target names, namespaces, and module interfaces.
3. **Report any deviations** — if you introduce behavior or interfaces that differ from the spec or architecture document, tell the user explicitly.
4. **Keep documentation updated** — if the architecture document differs from what you build (names, interfaces, module organization), update `docs/architecture.md` in the same commit.

---

## C++ Best Practices

### Code Style & Structure
- **Header Guards**: Always use `#pragma once` for header files
- **Namespaces**: Use `phu::sequencing` for reusable core, `phu::arpeggiator` for plugin-specific core, `phu::plugin` for JUCE adapter
- **Naming**: Use `camelCase` for functions, `PascalCase` for types, `snake_case` for variables, `UPPER_CASE` for constants
- **Include Order**: Group includes by module (sequencing/, arpeggiator/, JUCE), with blank lines between groups
- **No using namespace**: Never use `using namespace` in headers; use qualified names or `using` declarations in .cpp files only

### Type Safety & Modern C++
- **Strong Types**: Prefer `std::int64_t`, `std::uint8_t` over `int`/`unsigned` for specific sizes
- **constexpr**: Use `inline constexpr` for constants in `Constants.h` (single source of truth per §16.3)
- **noexcept**: Mark all [RT] functions `noexcept` (audio thread: no allocation, no locks, no blocking, no exceptions)
- **Bounds**: Validate all inputs; use `clampLaneParameters()` pattern for parameter validation
- **Optional**: Use `std::optional` for potentially absent values (e.g., `lastObservedSource_`)
- **Arrays**: Prefer `std::array<T, N>` over raw arrays with `inline constexpr` size

### Memory & Performance
- **No Allocation on [RT]**: Audio thread code must never allocate (use stacks, arenas, or pre-allocated buffers)
- **No Exceptions on [RT]**: Audio thread code must be `noexcept`
- **No Locks on [RT]**: Use lock-free structures like `TripleBuffer` for message↔audio communication
- **No Globals**: Avoid mutable global state; use dependency injection
- **No Singletons**: Avoid singletons with mutable state in core modules
- **No Randomness**: Core never uses random (spec has no random behavior; if added, use injected seeded generator)
- **Capacity Limits**: Respect `capacity::*` constants (e.g., `inputEventsPerChunk = 4096`)

### Error Handling
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

### CI/CD
- **Build Matrix**: All platforms (Windows x64, Linux x64/arm64, macOS x64/arm64)
- **Test Execution**: Run all tests on every push and PR
- **Validation**: Run pluginval on Windows and macOS builds
- **Artifacts**: Upload build artifacts for debugging

### Code Review
- **Spec Compliance**: Every change must be traceable to spec/arp-spec.md
- **Architecture Compliance**: Every change must follow docs/architecture.md
- **Module Ownership**: Changes to module Mxx must be reviewed against test-matrix.md
- **No Force Pushes**: Protect main branch; require PR reviews

---

## Copilot Cloud Harness Guardrails

### Pre-Commit Hooks (Recommended)
Create `.git/hooks/pre-commit` or use `pre-commit` framework:
```bash
#!/bin/sh
# Run clang-format check
git diff --cached --name-only | grep -E '\.(h|cpp)$' | xargs clang-format --dry-run --Werror
# Run tests
git stash push --keep-index
cmake --build --preset test
cmake --build --target test
git stash pop
```

### GitHub Actions Hooks
- **Required Checks**: Build, test, pluginval must pass before merge
- **Branch Protection**: Require status checks, reviews, and signed commits
- **CODEOWNERS**: Define code ownership for critical paths

### Custom Validation Scripts
- **Architecture Check**: Script to verify no JUCE headers in lib/
- **Dependency Check**: Script to verify module dependency order
- **RT Violations**: Script to detect allocation/locks in [RT] paths
- **Spec Compliance**: Script to check behavior against spec/arp-spec.md

### IDE Configuration
- **.editorconfig**: Consistent indentation, charset, line endings
- **.clang-tidy**: Static analysis rules
- **.clang-format**: Code formatting rules
- **compile_commands.json**: For IDE integration (enabled via `CMAKE_EXPORT_COMPILE_COMMANDS`)

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

---

## Quick Reference

| Concept | Location | Key File |
|---------|----------|----------|
| Spec | `spec/arp-spec.md` | §2-§21 |
| Architecture | `docs/architecture.md` | M01-M24 |
| Constants | `lib/sequencing/Constants.h` | All `inline constexpr` |
| Voice Management | `lib/sequencing/VoiceRegistry.h` | `startVoice`, `endVoice` |
| Triple Buffer | `lib/sequencing/TripleBuffer.h` | `writeBuffer`, `publish`, `update` |
| Lane Engine | `lib/arpeggiator/LaneEngine.h` | `processNextTimelineEvent` |
| Sequencer Core | `lib/arpeggiator/SequencerCore.h` | `processBlock` |
| Plugin Processor | `src/PluginProcessor.h` | JUCE adapter |
| Tests | `tests/` | Catch2 v3 |
| CI Config | `.github/workflows/` | Build, test, validate |

---

**Remember**: When in doubt, consult the spec first, then the architecture document. If behavior is ambiguous, flag it for user clarification.
