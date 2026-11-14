# Guardrails for phu-sequential

> **Reference only.** Agents: read only when changing CI, hooks or static/dynamic analysis. Day-to-day rules are in `.github/copilot-instructions.md` and `.github/instructions/cpp.instructions.md`.

This document describes the quality assurance mechanisms in place for the phu-sequential project, categorized into **AI guardrails** (for AI assistants like Copilot) and **non-AI guardrails** (for all contributors).

## Table of Contents

1. [AI Guardrails (Copilot Instructions)](#ai-guardrails-copilot-instructions)
2. [Static Analysis Guardrails](#static-analysis-guardrails)
3. [Dynamic Analysis Guardrails](#dynamic-analysis-guardrails)
4. [Build System Guardrails](#build-system-guardrails)
5. [Runtime Guardrails](#runtime-guardrails)
6. [Testing Guardrails](#testing-guardrails)
7. [CI/CD Guardrails](#cicd-guardrails)
8. [Code Review Guardrails](#code-review-guardrails)
9. [Git Hooks](#git-hooks)
10. [Recommended Additional Guardrails](#recommended-additional-guardrails)

---

## AI Guardrails (Copilot Instructions)

### Location
- `.github/copilot-instructions.md` - Main instructions file for GitHub Copilot

### What They Cover

The Copilot instructions provide AI assistants with:

1. **Primary References**
   - [spec/README.md](../spec/README.md) - Single source of truth for behavior
   - [docs/README.md](../docs/README.md) - Directory layout, targets, namespaces, interfaces

2. **C++ Best Practices**
   - Code style (naming, includes, namespaces)
   - Type safety (strong types, constexpr, noexcept)
   - Memory safety (no allocation on RT paths, no exceptions on RT)
   - Performance (bounds checking, capacity limits)
   - Error handling (assertions, validation)

3. **JUCE Best Practices**
   - Module isolation (no JUCE in lib/)
   - Audio processing patterns
   - Editor/UI guidelines
   - State management

4. **Architecture-Specific Rules**
   - Module boundaries (M01-M24)
   - Real-time guarantees ([RT] vs [MT] markers)
   - Data flow patterns
   - Testing strategies

5. **Quality Checklists**
   - Static analysis (compiler warnings, sanitizers)
   - Dynamic analysis (valgrind, pluginval)
   - CI/CD requirements
   - Code review checkpoints

6. **Common Pitfalls**
   - Floating point drift avoidance
   - Sample rounding
   - Voice management
   - Thread safety
   - Parameter validation
   - Grid change handling
   - Swing timing
   - Latch behavior
   - Discard revisions

### How They Work

GitHub Copilot reads `.github/copilot-instructions.md` and uses it to:
- Guide code generation
- Validate suggestions against project rules
- Provide context-aware completions
- Flag deviations from architecture

**Limitation**: AI guardrails are advisory only - they don't prevent bad code from being committed.

---

## Static Analysis Guardrails

### Compiler Warnings

**Location**: `cmake/compiler_flags.cmake`

**Configuration**:
```cmake
option(ENABLE_STRICT_WARNINGS "Enable strict compiler warnings" OFF)
```

**When Enabled**:
- **GCC/Clang**: `-Wall -Wextra -Wpedantic`
- **MSVC**: `/W4`
- **Disabled Warnings**: Unused parameters, deprecated declarations (common in JUCE)

**Usage**:
```bash
cmake --preset linux-x64-debug -DENABLE_STRICT_WARNINGS=ON
```

### Clang-Tidy (Recommended)

**Status**: Not yet configured (recommended addition)

**Recommended Configuration** (`.clang-tidy`):
```yaml
Checks: >
  - modernize-*
  - readability-*
  - performance-*
  - bugprone-*
  - clang-analyzer-*
  - cppcoreguidelines-*
WarningsAsErrors: ''
HeaderFilterRegex: ''
FormatStyle: file
```

**Key Checks for phu-sequential**:
- `modernize-use-noexcept` - Ensure [RT] functions are noexcept
- `performance-noexcept-move-constructor` - Move operations should be noexcept
- `bugprone-unchecked-optional-access` - Check optional access
- `cppcoreguidelines-avoid-magic-numbers` - Use constants from Constants.h
- `cppcoreguidelines-pro-type-member-init` - Initialize all members

### Clang-Format

**Status**: Not yet configured (recommended addition)

**Recommended Configuration** (`.clang-format`):
```yaml
BasedOnStyle: LLVM
IndentWidth: 4
TabWidth: 4
UseTab: Never
BreakBeforeBraces: Allman
AllowShortIfStatementsOnASingleLine: false
IndentCaseLabels: true
ColumnLimit: 120
```

**Usage**:
```bash
# Check formatting
clang-format --dry-run --Werror src/*.cpp lib/**/*.cpp

# Apply formatting
clang-format -i src/*.cpp lib/**/*.cpp
```

### EditorConfig

**Status**: Not yet configured (recommended addition)

**Recommended Configuration** (`.editorconfig`):
```ini
root = true

[*]
charset = utf-8
end_of_line = lf
indent_style = space
indent_size = 4
tab_width = 4
trim_trailing_whitespace = true
insert_final_newline = true

[*.{h,cpp,hpp,cc,cxx}]
indent_size = 4

[Makefile]
indent_style = tab
```

---

## Dynamic Analysis Guardrails

### Sanitizers

**Location**: `cmake/compiler_flags.cmake`

**Configuration**:
```cmake
option(ENABLE_SANITIZERS "Enable Address/Undefined Behavior Sanitizers" OFF)
```

**When Enabled**:
- **GCC/Clang**: `-fsanitize=address,undefined`
- **MSVC**: `/fsanitize=address`

**Usage**:
```bash
cmake --preset linux-x64-debug -DENABLE_SANITIZERS=ON
cmake --build --preset linux-x64-debug-build
ctest --output-on-failure
```

**Detects**:
- Use-after-free
- Heap buffer overflows
- Stack buffer overflows
- Memory leaks
- Undefined behavior (signed overflow, null deref, etc.)

### Valgrind

**Status**: Available in CI workflows (manual local use)

**Usage**:
```bash
valgrind --leak-check=full --error-exitcode=1 \
  ./build/tests/SequentialCoreTests
```

**CI Integration**:
```yaml
- name: Run Valgrind
  run: |
    valgrind --leak-check=full --error-exitcode=1 \
      ctest --output-on-failure
```

### Pluginval

**Location**: `.github/workflows/build-*.yml`

**Configuration**:
- Runs on Windows and macOS builds
- Strictness level: 5 (maximum)
- Validates VST3 plugin
- Checks for:
  - Memory leaks
  - Thread safety violations
  - Invalid MIDI events
  - Parameter automation issues
  - State serialization problems

**Usage**:
```bash
pluginval --strictness-level 5 --validate-in-process ./build/phu-sequential.vst3
```

---

## Build System Guardrails

### CMake Configuration

**Location**: `CMakeLists.txt`, `cmake/*.cmake`

**Key Guardrails**:

1. **Top-Level Guard**
   ```cmake
   if(NOT PROJECT_IS_TOP_LEVEL)
       return()
   endif()
   ```
   Prevents accidental inclusion as subproject

2. **Dependency Validation**
   ```cmake
   foreach(SUBMODULE IN ITEMS JUCE Catch2)
       if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${SUBMODULE}/CMakeLists.txt")
           message(FATAL_ERROR "${SUBMODULE} dependency not initialized")
       endif()
   endforeach()
   ```
   Ensures dependencies are properly cloned

3. **Language Standards**
   ```cmake
   set(CMAKE_CXX_STANDARD 20)
   set(CMAKE_CXX_STANDARD_REQUIRED ON)
   set(CMAKE_CXX_EXTENSIONS OFF)
   ```
   Enforces C++20, no compiler extensions

4. **JUCE Module Restrictions**
   **Location**: `cmake/juce_config.cmake`
   
   Disables unnecessary JUCE features:
   ```cmake
   JUCE_USE_CURL=0
   JUCE_WEB_BROWSER=0
   JUCE_USE_CAMERA=0
   JUCE_USE_MP3AUDIOFORMAT=0
   # ... etc
   ```
   Reduces attack surface and build time

5. **Platform-Specific Guards**
   ```cmake
   if(WIN32)
       add_compile_definitions(NOMINMAX)
   endif()
   ```
   Prevents Windows.h from defining min/max macros

### Build Presets

**Location**: `CMakePresets.json`

**Guardrails**:
- Consistent build configurations across platforms
- Separate presets for debug/release
- Platform-specific settings (architecture, generator)
- Test presets for running tests

**Example Preset**:
```json
{
  "name": "linux-x64-vst3-release",
  "displayName": "Linux x64 VST3 Release",
  "generator": "Ninja",
  "binaryDir": "${sourceDir}/build/linux-x64-vst3-release",
  "cacheVariables": {
    "CMAKE_BUILD_TYPE": "Release"
  },
  "vendor": {
    "microsoft.com/VisualStudioSettings/CMake/1.0": {
      "hostOS": ["Linux"]
    }
  }
}
```

---

## Runtime Guardrails

### Assertions

**Pattern**: Use `static_assert` for compile-time checks

**Example** (from Constants.h):
```cpp
static_assert(ticksPerWholeNote % (64 * 3) == 0);
static_assert((ticksPerWholeNote / 64) % 2 == 0);
static_assert(defaultLanePalette.size() == laneCount);
```

### Parameter Validation

**Pattern**: Centralized validation in `LaneConfiguration.h`

**Example**:
```cpp
[[nodiscard]] LaneParameters clampLaneParameters(const LaneParameters&) noexcept;
```

**Usage**: Always clamp parameters before use in audio thread

### Bounds Checking

**Pattern**: Use `std::array` with fixed sizes, validate indices

**Example** (from architecture.md):
```cpp
inline constexpr int laneCount = 8;
inline constexpr int maximumStepCount = 64;
```

**Usage**:
```cpp
if (laneIndex < 0 || laneIndex >= constants::lanes::laneCount) {
    // Handle error or clamp
}
```

### Capacity Limits

**Location**: `lib/sequencing/Constants.h` (capacity namespace)

**Key Limits**:
- `maximumSoundingVoices = 2048`
- `inputEventsPerChunk = 4096`
- `samplesPerChunk = 4096`
- `outputEventsPerBlock = 16384`
- `deferredEventCapacity = 8192`

**Purpose**: Prevents unbounded memory allocation in audio thread

### Real-Time Guarantees

**Pattern**: [RT] marker in architecture.md indicates:
- `noexcept`
- No allocation
- No locks
- No blocking
- O(bounded) complexity

**Enforcement**:
- Code review checks for [RT] violations
- Static analysis (clang-tidy) can detect allocation
- Sanitizers detect undefined behavior

---

## Testing Guardrails

### Unit Test Framework

**Framework**: Catch2 v3

**Location**: `tests/`

**Structure**:
- `tests/core/` - Tests for lib/sequencing/
- `tests/adapter/` - Tests for lib/arpeggiator/ and src/
- `tests/support/` - Test utilities (AllocationGuard, ScenarioRunner)

### Test Utilities

**AllocationGuard** (`tests/support/AllocationGuard.h`):
- Counts global `operator new` calls during a scope
- Verifies no allocation on [RT] paths
- Used in tests to validate real-time safety

**Example**:
```cpp
{
    AllocationGuard guard;
    // Code under test
    EXPECT(guard.getAllocationCount() == 0);
}
```

**ScenarioRunner** (`tests/support/ScenarioRunner.h`):
- Drives `SequencerCore` with scripted blocks
- Simulates transport, MIDI input, parameter changes
- Verifies output events match expected behavior

### Test Coverage

**Configuration**: `cmake/compiler_flags.cmake`

**Enable**:
```cmake
option(ENABLE_COVERAGE "Enable coverage flags" OFF)
```

**Usage**:
```bash
cmake --preset linux-x64-debug -DENABLE_COVERAGE=ON
cmake --build --preset linux-x64-debug-build
ctest --output-on-failure
lcov --capture --directory build --output-file coverage.info
genhtml coverage.info --output-directory coverage-report
```

**CI Integration**: Generate coverage report and upload as artifact

---

## CI/CD Guardrails

### GitHub Actions Workflows

**Location**: `.github/workflows/`

**Workflows**:
1. `build.yml` - Main build dispatcher
2. `build-linux.yml` - Linux build (x64, arm64)
3. `build-macos.yml` - macOS build (x64, arm64)
4. `build-windows-vst3.yml` - Windows build (x64)

**Guardrails in CI**:

1. **Build Matrix**
   - All platforms must build successfully
   - x64 and arm64 architectures
   - VST3 plugin format

2. **Dependency Installation**
   - Scripts for installing build dependencies
   - Example: `scripts/install-linux-deps.sh`

3. **Test Execution**
   ```yaml
   - name: Run Tests
     run: cmake --build --preset linux-x64-vst3-release-build --target test
   ```
   - All tests must pass

4. **Plugin Validation**
   ```yaml
   - name: Validate VST3 Plugin
     run: |
       pluginval --strictness-level 5 --validate "$VST3_PATH"
   ```
   - Strictness level 5 (maximum)
   - Runs on Windows and macOS

5. **Artifact Upload**
   - Build artifacts uploaded for debugging
   - Validation results uploaded on failure

6. **Branch Protection**
   - Main branch protected
   - Requires:
     - All CI checks pass
     - Code review approval
     - No force pushes

### Required Status Checks

**For Main Branch**:
- ✅ Build (all platforms)
- ✅ Tests (all platforms)
- ✅ Pluginval (Windows, macOS)

**For PRs**:
- Same as main branch
- Plus: Code review approval

---

## Code Review Guardrails

### Review Checklist

**For Every PR**:

1. **Spec Compliance**
   - [ ] Behavior matches [spec/README.md](../spec/README.md)
   - [ ] All spec sections referenced in code comments
   - [ ] Deviations documented and justified

2. **Architecture Compliance**
   - [ ] Follows [docs/README.md](../docs/README.md)
   - [ ] Module boundaries respected (M01-M24)
   - [ ] Dependency order maintained
   - [ ] No JUCE headers in lib/

3. **C++ Quality**
   - [ ] No raw pointers for ownership (use std::unique_ptr, std::shared_ptr)
   - [ ] All [RT] functions are noexcept
   - [ ] No allocation in [RT] paths
   - [ ] No locks in [RT] paths
   - [ ] Bounds checking on all array accesses
   - [ ] Parameter validation at module boundaries

4. **JUCE Usage**
   - [ ] JUCE confined to src/
   - [ ] No JUCE types in core interfaces
   - [ ] JUCE headers included only where necessary

5. **Testing**
   - [ ] New functionality has unit tests
   - [ ] Tests cover edge cases
   - [ ] Tests use AllocationGuard where appropriate
   - [ ] Existing tests still pass

6. **Documentation**
   - [ ] New interfaces documented
   - [ ] Architecture document updated if needed
   - [ ] Spec document updated if behavior changed

7. **Build System**
   - [ ] CMake configuration updated if needed
   - [ ] All presets still work
   - [ ] Dependencies properly declared

### CODEOWNERS

**Location**: `.github/CODEOWNERS` (recommended addition)

**Example**:
```
# Core modules
/lib/sequencing/ @huberp
/lib/arpeggiator/ @huberp

# JUCE adapter
/src/ @huberp

# Tests
/tests/ @huberp

# Documentation
docs/ @huberp
spec/ @huberp

# Build system
CMakeLists.txt @huberp
cmake/ @huberp
.github/workflows/ @huberp
```

**Purpose**: Automatically request reviews from code owners

---

## Git Hooks

### Pre-Commit Hook

**Location**: `.git/hooks/pre-commit` or via `pre-commit` framework

**Recommended Hooks**:

1. **Clang-Format Check**
   ```bash
   #!/bin/sh
   git diff --cached --name-only | grep -E '\.(h|cpp)$' | xargs clang-format --dry-run --Werror
   ```

2. **Tests Pass**
   ```bash
   #!/bin/sh
   git stash push --keep-index
   cmake --build --preset test 2>&1 | tee /tmp/test-output.txt
   if [ ${PIPESTATUS[0]} -ne 0 ]; then
       echo "Tests failed!"
       cat /tmp/test-output.txt
       git stash pop
       exit 1
   fi
   git stash pop
   ```

3. **Clang-Tidy Check**
   ```bash
   #!/bin/sh
   git diff --cached --name-only | grep -E '\.(h|cpp)$' | xargs clang-tidy
   ```

4. **Commit Message Format**
   ```bash
   #!/bin/sh
   if ! grep -qE '^[A-Za-z]+#[0-9]+:' <<< "$1"; then
       echo "Commit message must start with issue reference (e.g., 'Fixes #123:')"
       exit 1
   fi
   ```

### Pre-Push Hook

**Recommended**:
```bash
#!/bin/sh
# Run full test suite before push
cmake --build --preset test
ctest --output-on-failure
```

### Using pre-commit Framework

**Install**:
```bash
pip install pre-commit
```

**Configuration** (`.pre-commit-config.yaml`):
```yaml
repos:
  - repo: https://github.com/pre-commit/pre-commit-hooks
    rev: v4.4.0
    hooks:
      - id: trailing-whitespace
      - id: end-of-file-fixer
      - id: check-yaml
      - id: check-added-large-files

  - repo: https://github.com/Sarcasm/clang-format-hook
    rev: v1.0.0
    hooks:
      - id: clang-format
        types: [c++, c]

  - repo: local
    hooks:
      - id: tests
        name: Run Tests
        entry: cmake --build --preset test
        language: system
        pass_filenames: false
        stages: [push]
```

**Install Hooks**:
```bash
pre-commit install
pre-commit install --hook-type pre-push
```

---

## Recommended Additional Guardrails

### 1. Static Analysis Server

**Tool**: SonarQube or CodeQL

**Configuration**:
- Run on every PR
- Check for:
  - Code smells
  - Security vulnerabilities
  - Duplicated code
  - Complexity metrics

### 2. Spell Checking

**Tool**: codespell or misspell

**Configuration**:
```yaml
# .github/workflows/spellcheck.yml
- name: Spell Check
  run: |
    pip install codespell
    codespell --skip '*.vst3,*.dll,*.so' --ignore-words .codespell-ignore
```

### 3. Link Checking

**Tool**: lychee

**Configuration**:
```yaml
# .github/workflows/linkcheck.yml
- name: Link Check
  run: |
    npm install -g lychee
    lychee --no-progress .
```

### 4. License Compliance

**Tool**: FOSSA or Licensee

**Configuration**:
```yaml
# .github/workflows/license-check.yml
- name: License Check
  run: |
    docker run -v $(pwd):/src fossa/fossa analyze
```

### 5. Security Scanning

**Tool**: Trivy or Grype

**Configuration**:
```yaml
# .github/workflows/security-scan.yml
- name: Security Scan
  run: |
    docker run -v $(pwd):/src aquasec/trivy fs /src
```

### 6. Performance Regression Testing

**Tool**: Google Benchmark

**Integration**:
- Add benchmark targets
- Run benchmarks in CI
- Track performance over time
- Fail on significant regressions

---

## Guardrail Effectiveness Matrix

| Guardrail Type | Prevents | Detects | Fixes | Cost |
|---------------|----------|---------|-------|------|
| AI Instructions | ❌ | ✅ | ❌ | Low |
| Compiler Warnings | ✅ | ✅ | ❌ | Low |
| Static Analysis | ✅ | ✅ | ❌ | Medium |
| Sanitizers | ❌ | ✅ | ❌ | Medium |
| Unit Tests | ❌ | ✅ | ❌ | Medium |
| Pluginval | ❌ | ✅ | ❌ | High |
| CI/CD | ❌ | ✅ | ❌ | High |
| Code Review | ✅ | ✅ | ✅ | High |
| Git Hooks | ✅ | ✅ | ❌ | Low |

**Legend**:
- ✅ = Yes
- ❌ = No

---

## Implementation Roadmap

### Phase 1: Immediate (High Impact, Low Effort)
- [x] Enhanced copilot-instructions.md (DONE)
- [ ] Add `.editorconfig`
- [ ] Add `.clang-format`
- [ ] Configure clang-tidy
- [ ] Add pre-commit hooks

### Phase 2: Short Term (High Impact, Medium Effort)
- [ ] Add CODEOWNERS file
- [ ] Enable strict warnings in CI
- [ ] Add sanitizers to debug builds
- [ ] Add coverage reporting
- [ ] Add spell checking to CI

### Phase 3: Medium Term (Medium Impact, Higher Effort)
- [ ] Add SonarQube/CodeQL analysis
- [ ] Add performance benchmarks
- [ ] Add security scanning
- [ ] Add license compliance checking
- [ ] Add link checking

### Phase 4: Long Term (Strategic)
- [ ] Custom static analysis for [RT] violations
- [ ] Custom static analysis for module dependency violations
- [ ] Custom static analysis for spec compliance
- [ ] Automated documentation generation
- [ ] Automated changelog generation

---

## Summary

The phu-sequential project employs a **multi-layered guardrail strategy**:

1. **AI Guardrails** (Copilot Instructions) - Guide AI assistants
2. **Static Guardrails** (Compiler, Clang-Tidy, Clang-Format) - Catch issues at compile time
3. **Dynamic Guardrails** (Sanitizers, Valgrind, Pluginval) - Catch issues at runtime
4. **Build Guardrails** (CMake, Presets) - Ensure consistent builds
5. **Runtime Guardrails** (Assertions, Validation, Capacity Limits) - Ensure safe execution
6. **Testing Guardrails** (Unit Tests, Coverage) - Verify correctness
7. **CI/CD Guardrails** (GitHub Actions) - Enforce quality gates
8. **Review Guardrails** (Checklists, CODEOWNERS) - Ensure human oversight
9. **Git Hooks** (Pre-commit, Pre-push) - Catch issues before commit

**Mechanisms Beyond AI Guardrails**:
- ✅ **Git Hooks** - Pre-commit and pre-push validation
- ✅ **CI/CD Checks** - Build, test, validate on every push/PR
- ✅ **Static Analysis** - Compiler warnings, clang-tidy
- ✅ **Dynamic Analysis** - Sanitizers, valgrind, pluginval
- ✅ **Runtime Checks** - Assertions, parameter validation, capacity limits
- ✅ **Code Review** - Human oversight with checklists
- ✅ **Branch Protection** - Require checks and approvals

**For Copilot Cloud Harness**: GitHub provides **workflow hooks** that can be used to:
1. Run custom validation scripts before/after Copilot suggestions
2. Block suggestions that violate project rules
3. Log Copilot usage for auditing
4. Integrate with custom static analysis

However, as of 2024, Copilot Cloud Harness doesn't expose direct hook mechanisms for custom validation. The recommended approach is to:
1. Use enhanced instructions (copilot-instructions.md)
2. Rely on pre-commit hooks for local validation
3. Rely on CI/CD for remote validation
4. Use GitHub's built-in code scanning for security

---

## References

- [GitHub Copilot Documentation](https://docs.github.com/en/copilot)
- [Clang-Tidy Checks](https://clang.llvm.org/extra/clang-tidy/checks/list.html)
- [Clang-Format Style Options](https://clang.llvm.org/docs/ClangFormatStyleOptions.html)
- [Sanitizers Documentation](https://github.com/google/sanitizers)
- [Pluginval Documentation](https://github.com/Tracktion/pluginval)
- [Catch2 Documentation](https://github.com/catchorg/Catch2)
- [CMake Documentation](https://cmake.org/cmake/help/latest/)
- [JUCE Documentation](https://juce.com/learn/)
