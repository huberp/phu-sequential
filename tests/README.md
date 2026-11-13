# tests — Catch2 v3 test suite

**Namespace**: `phu::test`, `phu::arpeggiator::test`

## Layout & naming

| Item | Pattern |
|------|---------|
| Test files | `core/<Module>Tests.cpp` (e.g., `HeldNoteTrackerTests.cpp`) |
| Catch2 tags | `[Mxx][REQ-n.m-k][domain]` (e.g., `[M07][REQ-3.1-1][midi]`) |
| Support | `support/AllocationGuard.h`, `support/ScenarioRunner.h` |

- `Mxx`: Module ID (e.g., `M07` = HeldNoteTracker)
- `REQ-n.m-k`: Spec requirement (§n.m, item k); trace via `spec/arp-spec.md`

## Support utilities

**AllocationGuard**: Verifies zero heap allocations (REQ-17-1; real-time safety). Replaces `operator new/delete` with counters.
```cpp
phu::test::AllocationGuard guard;
scenarioRunner.runChunk(input);
REQUIRE(guard.allocationCount() == 0);
```

**ScenarioRunner**: Drives `SequencerCore` across chunks, accumulates absolute-sample MIDI log, enforces Guaranteed Note-Off (sec. 9.3).
```cpp
phu::arpeggiator::test::ScenarioRunner runner(config);
runner.runChunk(input);
REQUIRE(runner.allNotesClosed());
```

## Adding a test file

1. Create `tests/core/<ModuleName>Tests.cpp`
2. Register in `tests/CMakeLists.txt` target `SequentialCoreTests`
3. Include: `#include <catch2/catch_test_macros.hpp>` + module header
4. Tag tests: `TEST_CASE("...", "[Mxx][REQ-n.m-k]")` 
5. Build & run: `cmake --build --preset linux-x64-vst3-release-build --target SequentialCoreTests && ctest --test-dir build/linux-x64-vst3-release`

## Catch2 string_view rule

**Problem**: MSVC Catch2 v3 declares `StringMaker<std::string_view>` but doesn't link `convert()`, causing `LNK2019` on assertions that stringify `std::string_view`.

**Fix**: Wrap in `static_cast<std::string>`:
```cpp
std::string_view sv = "text";
CHECK(static_cast<std::string>(sv) == std::string{"text"});  // ✓ OK
```

**Enforced** by `scripts/check_catch2_string_view.cmake` (configure-time scan). See `.github/instructions/cpp.instructions.md` item 11–13.
