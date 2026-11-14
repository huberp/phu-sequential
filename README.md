# phu-sequential

A VST3 MIDI effect arpeggiator sequencer. See [spec/README.md](spec/README.md) for the complete specification.

## Prerequisites

- CMake 3.31.6 or later
- C++20 compatible compiler (GCC, Clang, or MSVC)
- For Linux: X11 development libraries (see `scripts/install-linux-deps.sh`)
- For macOS: Xcode Command Line Tools
- For Windows: Visual Studio 2022 with C++ workload

## Configure

```bash
# Linux x64
cmake --preset linux-x64-vst3-release

# macOS ARM64 (Apple Silicon)
cmake --preset macos-arm64-release

# Windows x64
cmake --preset windows-x64-vst3-release
```

## Build

```bash
# Linux x64
cmake --build --preset linux-x64-vst3-release-build

# macOS ARM64
cmake --build --preset macos-arm64-release-build

# Windows x64
cmake --build --preset windows-x64-vst3-release-build
```

## Test

```bash
# Run Catch2 tests for the core library
cmake --build --preset linux-x64-vst3-release-build --target SequentialCoreTests

# Run all tests (no ctest presets exist; use --test-dir)
ctest --test-dir build/linux-x64-vst3-release --output-on-failure
```

## Linux-Specific Setup

If you're on Linux, first install the required dependencies:

```bash
sudo bash scripts/install-linux-deps.sh
```

This installs JUCE build requirements: X11 libraries, ALSA, Jack, FreeType, and Fontconfig.

## Project Layout

- `lib/` — Reusable core library without JUCE (SequentialCoreLib)
- `src/` — JUCE plugin adapter (phu-sequential VST3/AU)
- `tests/` — Catch2 tests for the core library
- `cmake/` — Build configuration (JUCE, Catch2, compiler flags)
- `.github/workflows/` — CI on Windows, macOS, Linux

## Documentation

- `spec/arp-spec.md` — Complete plugin specification
- `docs/architecture.md` — Code architecture and module organization
