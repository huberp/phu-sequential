# Copilot Instructions for phu-sequential

VST3 MIDI arpeggiator sequencer (C++20, JUCE only in `src/`, Catch2 v3 tests).

## Rules for every change

1. **Spec first**: `spec/arp-spec.md` is the source of truth (cited as `§n`). It wins over every other document.
2. **Architecture**: `docs/architecture.md` defines layout, target names, namespaces and module interfaces (modules `M01`–`M24`).
3. **Report deviations** from spec/architecture to the user explicitly.
4. **Keep docs in sync**: if you change names, interfaces or module organization, update `docs/architecture.md` (and `docs/module-map.md` if files/tests move) in the same commit.
5. C++/JUCE rules (RT safety, namespaces, Catch2 pitfalls): `.github/instructions/cpp.instructions.md` (auto-applied to C++ files).
6. All code, tests and scripts must work on every CI platform (Windows x64, Linux x64/arm64, macOS x64/arm64): CMake-native/cross-platform mechanisms only. See `.github/workflows/`.

## Navigate cheaply (do NOT read whole docs)

`docs/architecture.md` (~42k tokens), `docs/test-matrix.md` (~9k) and `spec/arp-spec.md` (~7k) are large. Read only what you need:

1. Open `docs/module-map.md` — one row per module: architecture heading, source files, tests, spec §, decisions D-nn, dependencies.
2. Find the section by heading, then read only that range:
   `grep -n '^### 4.3' docs/architecture.md` → `view` with `view_range`.
3. Spec sections: `grep -n '^## ' spec/arp-spec.md`; test requirements: `grep -n '§9' docs/test-matrix.md` (rows grouped by spec §).
4. Decisions: `grep -n 'D-14' docs/architecture.md` (index in architecture §10).
5. Per-directory orientation: `lib/sequencing/README.md`, `lib/arpeggiator/README.md`, `src/README.md`, `tests/README.md`.

`.github/COPILOT_GUARDRAILS_SUMMARY.md` and `docs/guardrails.md` are background reference only; read them only when changing CI, hooks or static analysis.

## Build and test (Linux x64)

```bash
cmake --preset linux-x64-vst3-release
cmake --build --preset linux-x64-vst3-release-build --target SequentialCoreTests   # core tests only (skips plugin)
ctest --test-dir build/linux-x64-vst3-release --output-on-failure                  # all tests (no ctest presets exist)
ctest --test-dir build/linux-x64-vst3-release -R "REQ-4"                           # subset by test name/tag regex
cmake -P scripts/check_catch2_string_view.cmake        # Catch2 guard (also runs as a post-tool hook)
```

macOS/Windows presets: see `README.md`.

## Recommended workflow for a module issue

Tests first, then header/skeleton, then implementation (TDD). Delegating those steps to cheap subagents (e.g. claude-haiku-4.5) worked well (PR #25); then review and fix manually. Tick the module's `todo` rows in `docs/test-matrix.md` in the same PR.

## Layout

`lib/sequencing/` (reusable core, `phu::sequencing`) → `lib/arpeggiator/` (`phu::arpeggiator`) → `src/` (JUCE adapter, `phu::plugin`). `tests/core`, `tests/support`. `docs/`, `spec/`, `cmake/`, `.github/workflows/`.
