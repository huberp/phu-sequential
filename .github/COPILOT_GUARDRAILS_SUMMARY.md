# Copilot Guardrails Summary for phu-sequential

> **Reference only — not needed for normal coding tasks.** Agents: do not read this unless changing CI, hooks or tooling. Authoritative rules live in `.github/copilot-instructions.md` and `.github/instructions/cpp.instructions.md`.

## Quick Answer to Your Questions

### Q1: What AI-guardrails exist and what should be added?

**Existing**: `.github/copilot-instructions.md` only referenced architecture.md

**Now Enhanced** with:
- ✅ C++ best practices (modern C++20, type safety, noexcept, no allocation on RT)
- ✅ JUCE best practices (module isolation, no JUCE in lib/, minimal features)
- ✅ Architecture-specific rules (M01-M24 modules, [RT] vs [MT] markers)
- ✅ Real-time guarantees (no allocation, no locks, no blocking on audio thread)
- ✅ Testing strategies (Catch2, AllocationGuard, ScenarioRunner)
- ✅ Common pitfalls (floating point drift, sample rounding, voice management)
- ✅ Quality checklists (static analysis, dynamic analysis, CI/CD)

**Location**: `.github/copilot-instructions.md` (10KB, comprehensive)

### Q2: Are there mechanisms beyond AI-guardrails for Copilot Cloud Harness?

**Short Answer**: GitHub Copilot Cloud Harness has **limited direct hook mechanisms**, but you can implement guardrails at multiple levels:

---

## Guardrail Mechanisms Beyond AI Instructions

### 1. Git Hooks (Local, Pre-Commit/Push)

**What**: Custom scripts that run before commit or push

**Mechanisms**:
```bash
# .git/hooks/pre-commit
#!/bin/sh
# 1. Check code formatting
git diff --cached --name-only | grep -E '\.(h|cpp)$' | xargs clang-format --dry-run --Werror

# 2. Run tests
git stash push --keep-index
cmake --build --preset test
ctest --output-on-failure
if [ $? -ne 0 ]; then
    git stash pop
    exit 1
fi
git stash pop

# 3. Run static analysis
clang-tidy $(git diff --cached --name-only | grep -E '\.(h|cpp)$')
```

**Tools**:
- `pre-commit` framework (Python-based, cross-platform)
- Custom shell scripts
- `lefthook` (faster alternative to pre-commit)

**Effectiveness**: ✅✅✅ (Prevents bad code from being committed locally)

### 2. CI/CD Checks (Remote, GitHub Actions)

**What**: Automated validation on every push and PR

**Current Implementation**:
- ✅ Build (all platforms: Windows, Linux x64/arm64, macOS x64/arm64)
- ✅ Tests (Catch2 unit tests)
- ✅ Pluginval validation (VST3, strictness level 5)
- ✅ Branch protection (require checks to pass)

**Enhanced Checks to Add**:
```yaml
# .github/workflows/quality.yml
- name: Clang-Tidy
  run: |
    find lib src -name "*.cpp" -o -name "*.h" | xargs clang-tidy

- name: Clang-Format
  run: |
    find lib src -name "*.cpp" -o -name "*.h" | xargs clang-format --dry-run --Werror

- name: Sanitizers
  run: |
    cmake --preset linux-x64-debug -DENABLE_SANITIZERS=ON
    cmake --build --preset linux-x64-debug-build
    ctest

- name: Coverage
  run: |
    cmake --preset linux-x64-debug -DENABLE_COVERAGE=ON
    cmake --build --preset linux-x64-debug-build
    ctest
    lcov --capture --directory build
```

**Effectiveness**: ✅✅✅✅ (Prevents bad code from being merged)

### 3. Static Analysis (Compile-Time)

**What**: Compiler and tool-based code analysis

**Mechanisms**:

| Tool | Purpose | When to Run |
|------|---------|--------------|
| Compiler Warnings | Basic issues | Every build |
| Clang-Tidy | Modern C++ checks | Pre-commit, CI |
| Clang-Format | Code style | Pre-commit, CI |
| Cppcheck | Static analysis | CI |
| PVS-Studio | Deep static analysis | Nightly |

**Configuration** (cmake/compiler_flags.cmake):
```cmake
option(ENABLE_STRICT_WARNINGS "Enable strict compiler warnings" ON)
option(ENABLE_SANITIZERS "Enable Address/Undefined Behavior Sanitizers" ON)
```

**Effectiveness**: ✅✅✅ (Catches issues before runtime)

### 4. Dynamic Analysis (Runtime)

**What**: Runtime error detection

**Mechanisms**:

| Tool | Purpose | When to Run |
|------|---------|--------------|
| AddressSanitizer | Memory errors | Debug builds |
| UndefinedBehaviorSanitizer | UB | Debug builds |
| Valgrind | Memory leaks | CI (Linux) |
| Pluginval | VST3 validation | CI (Windows/macOS) |

**Configuration**:
```bash
# Enable sanitizers
cmake -DENABLE_SANITIZERS=ON
cmake --build

# Run with valgrind
valgrind --leak-check=full ./tests/SequentialCoreTests

# Run pluginval
pluginval --strictness-level 5 ./build/phu-sequential.vst3
```

**Effectiveness**: ✅✅✅✅ (Catches runtime issues)

### 5. Runtime Guardrails (In-Code)

**What**: Safety checks embedded in the code

**Mechanisms**:

1. **Assertions**
   ```cpp
   static_assert(ticksPerWholeNote % (64 * 3) == 0);
   assert(laneIndex >= 0 && laneIndex < constants::lanes::laneCount);
   ```

2. **Parameter Validation**
   ```cpp
   LaneParameters clamped = clampLaneParameters(params);
   ```

3. **Bounds Checking**
   ```cpp
   if (stepIndex < 0 || stepIndex >= constants::lanes::maximumStepCount) {
       return; // or clamp
   }
   ```

4. **Capacity Limits**
   ```cpp
   inline constexpr int maximumSoundingVoices = 2048;
   inline constexpr int inputEventsPerChunk = 4096;
   ```

5. **Real-Time Guarantees**
   ```cpp
   // [RT] marker means: noexcept, no allocation, no locks, O(bounded)
   void processBlock(...) noexcept;
   ```

**Effectiveness**: ✅✅✅ (Prevents undefined behavior)

### 6. Testing Guardrails

**What**: Automated test validation

**Mechanisms**:

1. **Unit Tests** (Catch2)
   ```cpp
   TEST_CASE("StepTiming calculations") {
       // Test logic
   }
   ```

2. **AllocationGuard**
   ```cpp
   {
       AllocationGuard guard;
       core.processBlock(...);
       EXPECT(guard.getAllocationCount() == 0);
   }
   ```

3. **ScenarioRunner**
   ```cpp
   ScenarioRunner runner;
   runner.addEvents(...);
   runner.run(core);
   runner.verifyOutput(...);
   ```

4. **Coverage**
   ```bash
   cmake -DENABLE_COVERAGE=ON
   ctest
   lcov --capture
   ```

**Effectiveness**: ✅✅✅✅ (Validates correctness)

### 7. Code Review Guardrails

**What**: Human oversight with structured checklists

**Mechanisms**:

1. **Review Checklist** (see docs/guardrails.md)
   - [ ] Spec compliance
   - [ ] Architecture compliance
   - [ ] C++ quality
   - [ ] JUCE usage
   - [ ] Testing
   - [ ] Documentation

2. **CODEOWNERS**
   ```
   # .github/CODEOWNERS
   /lib/ @huberp
   /src/ @huberp
   ```

3. **Branch Protection**
   - Require reviews
   - Require status checks
   - Require signed commits
   - Block force pushes

**Effectiveness**: ✅✅✅✅ (Final quality gate)

---

## Copilot Cloud Harness Specifics

### Current State (2024)

GitHub Copilot Cloud Harness **does not expose direct hook APIs** for custom validation. However, you can implement guardrails at these integration points:

### 1. Copilot Chat Extensions

**What**: Custom slash commands in Copilot Chat

**Mechanism**:
- Create GitHub App with custom slash commands
- Commands can trigger validation scripts
- Example: `/validate-cpp` runs clang-tidy on current file

**Effectiveness**: ✅ (Manual trigger, advisory)

### 2. Copilot Workspace (VS Code)

**What**: VS Code extension that validates Copilot suggestions

**Mechanisms**:
- **Extension API**: Create extension that listens to Copilot suggestions
- **Validation**: Run clang-tidy, clang-format on suggestions before acceptance
- **Block/Allow**: Reject suggestions that violate rules

**Example Extension**:
```typescript
// extension.ts
import * as vscode from 'vscode';
import { execSync } from 'child_process';

export function activate(context: vscode.ExtensionContext) {
    vscode.workspace.onDidInsertCompletionText(async (e) => {
        const document = vscode.window.activeTextEditor?.document;
        if (!document || !document.fileName.endsWith('.cpp')) return;
        
        // Get the inserted text
        const range = new vscode.Range(
            e.position.line, e.position.character - e.text.length,
            e.position.line, e.position.character
        );
        const text = document.getText(range);
        
        // Validate with clang-format
        try {
            execSync('echo ' + JSON.stringify(text) + ' | clang-format --dry-run --Werror', 
                     { encoding: 'utf-8' });
        } catch (error) {
            vscode.window.showWarningMessage(
                'Copilot suggestion violates code style. Please reformat.'
            );
            // Optionally: undo the insertion
            await vscode.window.activeTextEditor?.edit(editBuilder => {
                editBuilder.delete(range);
            });
        }
    });
}
```

**Effectiveness**: ✅✅ (Prevents bad suggestions from being accepted)

### 3. GitHub Actions + Copilot

**What**: Validate Copilot-generated PRs

**Mechanism**:
```yaml
# .github/workflows/copilot-review.yml
name: Copilot PR Review

on:
  pull_request:
    types: [opened, synchronize, ready_for_review]

jobs:
  validate:
    if: github.event.pull_request.user.login == 'github-copilot' || 
        contains(github.event.pull_request.body, '[Copilot]')
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v7
      
      - name: Check code style
        run: |
          find lib src -name "*.cpp" -o -name "*.h" | xargs clang-format --dry-run --Werror
      
      - name: Run static analysis
        run: |
          find lib src -name "*.cpp" -o -name "*.h" | xargs clang-tidy
      
      - name: Build and test
        run: |
          cmake --preset linux-x64-debug
          cmake --build --preset linux-x64-debug-build
          ctest --output-on-failure
```

**Effectiveness**: ✅✅✅ (Validates Copilot-generated code before merge)

### 4. Custom Copilot Instructions Per File

**What**: File-specific instructions for Copilot

**Mechanism**:
- Add comments at top of files:
  ```cpp
  // COPILOT: This is M01 Constants module. Only use inline constexpr. No JUCE headers.
  // COPILOT: All values must match spec/arp-spec.md
  ```
- Copilot reads these and adjusts suggestions

**Effectiveness**: ✅ (Advisory, file-specific)

---

## Recommended Implementation Plan

### For Immediate Protection (This Week)

1. **Enhance Copilot Instructions** ✅ DONE
   - File: `.github/copilot-instructions.md`
   - Content: C++, JUCE, architecture, testing, pitfalls

2. **Add Pre-Commit Hooks**
   ```bash
   # Install pre-commit framework
   pip install pre-commit
   
   # Create .pre-commit-config.yaml
   repos:
     - repo: https://github.com/pre-commit/pre-commit-hooks
       rev: v4.4.0
       hooks:
         - id: trailing-whitespace
         - id: end-of-file-fixer
     - repo: https://github.com/Sarcasm/clang-format-hook
       rev: v1.0.0
       hooks:
         - id: clang-format
   
   # Install hooks
   pre-commit install
   ```

3. **Enable Strict Warnings in CI**
   ```yaml
   # In build workflows
   - name: Configure CMake
     run: cmake --preset linux-x64-debug -DENABLE_STRICT_WARNINGS=ON
   ```

### For Strong Protection (This Month)

4. **Add Static Analysis to CI**
   ```yaml
   - name: Clang-Tidy
     run: |
       find lib src -name "*.cpp" -o -name "*.h" | xargs clang-tidy
   ```

5. **Add CODEOWNERS**
   ```
   # .github/CODEOWNERS
   * @huberp
   ```

6. **Enable Branch Protection**
   - Settings > Branches > Branch protection rules
   - Require status checks
   - Require reviews
   - Block force pushes

### For Comprehensive Protection (This Quarter)

7. **Add Sanitizers to Debug Builds**
   ```yaml
   - name: Build with Sanitizers
     run: |
       cmake --preset linux-x64-debug -DENABLE_SANITIZERS=ON
       cmake --build --preset linux-x64-debug-build
       ctest
   ```

8. **Add Coverage Reporting**
   ```yaml
   - name: Generate Coverage
     run: |
       cmake --preset linux-x64-debug -DENABLE_COVERAGE=ON
       cmake --build --preset linux-x64-debug-build
       ctest
       lcov --capture --directory build
   ```

9. **Create VS Code Extension for Copilot Validation**
   - Validate suggestions with clang-format
   - Validate suggestions with clang-tidy
   - Block non-compliant suggestions

---

## Guardrail Effectiveness Comparison

| Mechanism | Prevents Bad Code | Detects Issues | Auto-Fixes | Copilot-Specific | Effort |
|-----------|-------------------|----------------|------------|------------------|--------|
| AI Instructions | ❌ | ✅ | ❌ | ✅ | Low |
| Pre-Commit Hooks | ✅ | ✅ | ❌ | ❌ | Low |
| CI/CD Checks | ❌ | ✅ | ❌ | ❌ | Medium |
| Static Analysis | ✅ | ✅ | ❌ | ❌ | Medium |
| Dynamic Analysis | ❌ | ✅ | ❌ | ❌ | Medium |
| Runtime Checks | ✅ | ✅ | ❌ | ❌ | Low |
| Unit Tests | ❌ | ✅ | ❌ | ❌ | Medium |
| Code Review | ✅ | ✅ | ✅ | ❌ | High |
| VS Code Extension | ✅ | ✅ | ✅ | ✅ | High |
| Copilot Chat Commands | ❌ | ✅ | ❌ | ✅ | Medium |

**Recommendation**: Use a **layered approach** combining multiple mechanisms for defense in depth.

---

## Files Created/Modified

1. **Modified**: `.github/copilot-instructions.md` (10KB)
   - Added C++ best practices
   - Added JUCE best practices
   - Added architecture-specific rules
   - Added common pitfalls
   - Added quality checklists

2. **Created**: `docs/guardrails.md` (20KB)
   - Comprehensive guardrail documentation
   - Static analysis configuration
   - Dynamic analysis configuration
   - Build system guardrails
   - Runtime guardrails
   - Testing guardrails
   - CI/CD guardrails
   - Code review guardrails
   - Git hooks
   - Implementation roadmap

3. **Created**: `.github/COPILOT_GUARDRAILS_SUMMARY.md` (this file)
   - Quick reference for Copilot-specific guardrails
   - Comparison of mechanisms
   - Implementation plan

---

## Next Steps

1. **Review** the enhanced copilot-instructions.md
2. **Implement** pre-commit hooks (start with clang-format)
3. **Enable** strict warnings in CI
4. **Add** CODEOWNERS file
5. **Enable** branch protection
6. **Consider** VS Code extension for Copilot validation

---

## References

- [GitHub Copilot Documentation](https://docs.github.com/en/copilot)
- [GitHub Actions Documentation](https://docs.github.com/en/actions)
- [Clang-Tidy Documentation](https://clang.llvm.org/extra/clang-tidy/)
- [Pre-Commit Framework](https://pre-commit.com/)
- [VS Code Extension API](https://code.visualstudio.com/api)
