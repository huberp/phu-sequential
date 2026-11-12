# Copilot Instructions for phu-sequential

**When making changes to the plugin, always:**

1. **Consult `spec/arp-spec.md`** — the single source of truth for plugin behavior and architecture (§2, §15, §16, §18).
2. **Consult `docs/architecture.md`** — the specification for directory layout, target names, namespaces, and module interfaces.
3. **Report any deviations** — if you introduce behavior or interfaces that differ from the spec or architecture document, tell the user explicitly.
4. **Keep documentation updated** — if the architecture document differs from what you build (names, interfaces, module organization), update `docs/architecture.md` in the same commit.

---

## Project Structure

- **Repository Root**: `phu-sequential/`
- **Core Libraries**: `lib/sequencing/` (reusable), `lib/arpeggiator/` (plugin-specific)
- **JUCE Adapter**: `src/` (plugin implementation)
- **Tests**: `tests/` (Catch2 v3)
- **Documentation**: `docs/`, `spec/`
- **Build System**: `CMakeLists.txt`, `cmake/`
- **CI/CD**: `.github/workflows/`

---

## C++ and JUCE Specific Instructions

For C++ and JUCE best practices, see the path-specific instructions file:
- `.github/instructions/cpp.instructions.md` (applies to all `.cpp`, `.h`, `.hpp`, `.cc`, `.cxx` files)

This file contains detailed guidance on:
- C++ code style and structure
- Type safety and modern C++ practices
- Memory and performance constraints (especially for real-time audio thread)
- JUCE framework usage rules
- Architecture-specific module boundaries
- Testing strategies
- Quality guardrails
- Common pitfalls

---

## General Quality Guardrails

### CI/CD

- **Build Matrix**: All platforms (Windows x64, Linux x64/arm64, macOS x64/arm64)
- **Platform Rule**: All code, including tests and scripts, must work on every platform in the build matrix. Use CMake-native or cross-platform mechanisms only; never depend on a specific interpreter path or platform-only tooling. Verify against `.github/workflows/`.
- **Guard Hook**: `.github/hooks/catch2-string-view-guard.json` runs `cmake -P scripts/check_catch2_string_view.cmake` after every tool call (Catch2 string_view linker guard, cpp.instructions.md item 11)
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

## Quick Reference

| Concept | Location | Key File |
|---------|----------|----------|
| Spec | `spec/arp-spec.md` | §2-§21 |
| Architecture | `docs/architecture.md` | M01-M24 |
| Tests | `tests/` | Catch2 v3 |
| CI Config | `.github/workflows/` | Build, test, validate |
| C++ Instructions | `.github/instructions/cpp.instructions.md` | C++/JUCE best practices |

---

**Remember**: When in doubt, consult the spec first, then the architecture document. If behavior is ambiguous, flag it for user clarification.
