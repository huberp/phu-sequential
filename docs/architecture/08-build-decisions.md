# phu-sequential — Architecture — 08-build-decisions.md

## 8. Build decisions (§15) — D-38

### 8.1 Versions (verified against upstream tags)

| Item | Decision |
|---|---|
| CMake | `cmake_minimum_required(VERSION 3.31.6)` (tag `v3.31.6` exists; same as template) |
| JUCE | `9.0.5` (latest 9.x release; no reason to stay on 9.0.4). JUCE 9 on Linux needs `libxi-dev` in addition to the template's packages |
| Catch2 | `v3.16.1` (latest release tag at writing; requires CMake ≥ 3.16), `Catch2::Catch2WithMain`, `catch_discover_tests` |
| C++ | C++20 (`CMAKE_CXX_STANDARD 20`, `CMAKE_CXX_STANDARD_REQUIRED ON`, `CMAKE_CXX_EXTENSIONS OFF`); uses `std::span`, defaulted `operator==` |
| Compilers / CI | MSVC (VS 2026, `windows-latest`), GCC (`ubuntu-latest`, x64) + `ubuntu-24.04-arm`, AppleClang (`macos-latest` arm64, `macos-15-intel` x64); macOS deployment target 11.0 |
| Formats | VST3 everywhere (§2); AU (`AU_MAIN_TYPE kAudioUnitType_MIDIProcessor`) additionally on macOS as an optional extra (D-39); Standalone not built |
| phu-audio-lib | not used (no audio processing path) |

### 8.2 CMake structure (mirrors huberp/phu-bark-fft-compressor)

```cmake
# CMakeLists.txt (root)
cmake_minimum_required(VERSION 3.31.6)
project(phu-sequential VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(compiler_flags)              # ENABLE_STRICT_WARNINGS / ENABLE_SANITIZERS / ENABLE_COVERAGE
include(PhuDependencies)
option(PHU_BUILD_TESTS "Build unit tests" ${PROJECT_IS_TOP_LEVEL})
phu_fetch_juce(TAG 9.0.5)
include(juce_config)                 # JUCE_BUILD_EXTRAS/EXAMPLES OFF, compile definitions
add_subdirectory(lib)                # SequentialCoreLib — no JUCE link
add_subdirectory(src)                # phu-sequential plugin
if(PHU_BUILD_TESTS)
  phu_fetch_catch2(TAG v3.16.1)
  enable_testing()
  add_subdirectory(tests)
endif()
```

```cmake
# lib/CMakeLists.txt
add_library(SequentialCoreLib STATIC)            # D-2
target_sources(SequentialCoreLib PRIVATE sequencing/StepTiming.cpp …)   # each issue adds its units
target_include_directories(SequentialCoreLib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(SequentialCoreLib PUBLIC cxx_std_20)
phu_apply_compiler_flags(SequentialCoreLib)      # from compiler_flags.cmake
```

```cmake
# src/CMakeLists.txt
set(PHU_PLUGIN_FORMATS "VST3" CACHE STRING "Plugin formats")   # presets add AU on macOS
juce_add_plugin(phu-sequential
  COMPANY_NAME "phu"  PRODUCT_NAME "PHU SEQUENTIAL"
  PLUGIN_MANUFACTURER_CODE Phub  PLUGIN_CODE Pseq
  IS_SYNTH FALSE  IS_MIDI_EFFECT TRUE  NEEDS_MIDI_INPUT TRUE  NEEDS_MIDI_OUTPUT TRUE
  EDITOR_WANTS_KEYBOARD_FOCUS TRUE                # text-grid field
  VST3_CATEGORIES "Fx" "Tools"                    # D-39: VST3 has no MIDI-effect category
  AU_MAIN_TYPE kAudioUnitType_MIDIProcessor
  COPY_PLUGIN_AFTER_BUILD FALSE
  FORMATS ${PHU_PLUGIN_FORMATS})
file(GLOB PHU_FACTORY_PRESETS CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/presets/factory/*.phuseq)   # M24 (#16)
list(SORT PHU_FACTORY_PRESETS)
target_sources(phu-sequential PRIVATE PluginProcessor.cpp …)
target_compile_definitions(phu-sequential PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(phu-sequential PRIVATE SequentialCoreLib
  juce::juce_audio_utils juce::juce_audio_processors juce::juce_gui_basics juce::juce_data_structures
  PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)
if(PHU_FACTORY_PRESETS)              # juce_add_binary_data rejects an empty SOURCES list (before #16)
  juce_add_binary_data(SequentialFactoryPresets SOURCES ${PHU_FACTORY_PRESETS})
  target_link_libraries(phu-sequential PRIVATE SequentialFactoryPresets)
  target_compile_definitions(phu-sequential PRIVATE PHU_HAS_FACTORY_PRESETS=1)
else()
  target_compile_definitions(phu-sequential PRIVATE PHU_HAS_FACTORY_PRESETS=0)
endif()
```

```cmake
# tests/CMakeLists.txt
add_executable(SequentialCoreTests support/AllocationGuard.cpp support/ScenarioRunner.cpp core/…Tests.cpp)
target_link_libraries(SequentialCoreTests PRIVATE SequentialCoreLib Catch2::Catch2WithMain)   # no JUCE (§18)
catch_discover_tests(SequentialCoreTests)
# Adapter tests: compile the JUCE-dependent units under test directly (no plugin wrapper)
juce_add_console_app(SequentialAdapterTests)
target_sources(SequentialAdapterTests PRIVATE ../src/MidiConversion.cpp ../src/HostTransport.cpp
  ../src/ParameterIds.cpp ../src/ParameterLayout.cpp ../src/ParameterReader.cpp ../src/PatternStore.cpp
  ../src/StateSerializer.cpp ../src/StateMigration.cpp ../src/PresetManager.cpp adapter/…Tests.cpp)
target_link_libraries(SequentialAdapterTests PRIVATE SequentialCoreLib Catch2::Catch2WithMain
  juce::juce_audio_basics juce::juce_audio_processors juce::juce_data_structures)
if(TARGET SequentialFactoryPresets)
  target_link_libraries(SequentialAdapterTests PRIVATE SequentialFactoryPresets)
  target_compile_definitions(SequentialAdapterTests PRIVATE PHU_HAS_FACTORY_PRESETS=1)
else()
  target_compile_definitions(SequentialAdapterTests PRIVATE PHU_HAS_FACTORY_PRESETS=0)
endif()
catch_discover_tests(SequentialAdapterTests)
```

`cmake/PhuDependencies.cmake` keeps the template's `_phu_fetch_dependency(NAME GIT_REPOSITORY GIT_TAG)` macro (FetchContent, `GIT_SHALLOW TRUE`, `SOURCE_DIR ${PROJECT_SOURCE_DIR}/<name>` git-ignored) and adds `phu_fetch_catch2(TAG …)` (`https://github.com/catchorg/Catch2.git`, then `list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)` + `include(Catch)`).

### 8.3 Presets and CI

- `CMakePresets.json` v5, hidden mixins `os-<platform>-<arch>`, `build-<debug|release>`, `formats-<vst3|vst3-au>`; visible configure presets `<os>-<arch>-<formats>-<config>` (e.g. `linux-x64-vst3-release`, `macos-arm64-vst3-au-release`, `windows-x64-vst3-debug`) and matching `-build` and `-test` presets. Generators: Windows "Visual Studio 18 2026", macOS Xcode, Linux Ninja.
- Workflows: `build.yml` → reusable `build-windows-vst3.yml`, `build-linux.yml`, `build-macos.yml`; each configures, builds, runs `ctest --output-on-failure`; pluginval (latest release zip, `--strictness-level 10`) on Windows and macOS (§15 test in CI); Linux runs unit tests only (plus pluginval optional). Artifacts: VST3 bundles (and AU on macOS).

---

