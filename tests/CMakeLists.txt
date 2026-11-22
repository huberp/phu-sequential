add_executable(SequentialCoreTests
    support/AllocationGuard.cpp
    support/ScenarioRunner.cpp
    core/PlaceholderTests.cpp
    core/ConstantsTests.cpp
    core/MusicalTimeTests.cpp
    core/NoteValuesTests.cpp
    core/StepTimingTests.cpp
    core/SampleConversionTests.cpp
    core/DisplayScaleTests.cpp
    core/TextGridParserTests.cpp
    core/MidiEventTests.cpp
    core/HeldNoteTrackerTests.cpp
    core/SourceSelectionTests.cpp
    core/VelocityModesTests.cpp
    core/StepDataTests.cpp
    core/OutputEventBufferTests.cpp
    core/VoiceRegistryTests.cpp
    core/LaneConfigurationTests.cpp
    core/LanePositionTests.cpp
    core/LaneEngineTests.cpp
    core/TransportTrackerTests.cpp
    core/PassThroughFilterTests.cpp
    core/SequencerCoreTests.cpp
    core/ConfigurationBuilderTests.cpp
    core/PatternDocumentTests.cpp
    core/LaneStateTests.cpp
    core/UndoManagerTests.cpp
    core/LaneCommandTests.cpp
    core/TripleBufferTests.cpp
)

target_include_directories(SequentialCoreTests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(SequentialCoreTests PRIVATE
    SequentialCoreLib
    Catch2::Catch2WithMain
)

set_target_properties(SequentialCoreTests PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# Enable ctest discovery for Catch2
include(Catch)
catch_discover_tests(SequentialCoreTests)

# Catch2 string_view linker guard for SequentialCoreTests
execute_process(
    COMMAND ${CMAKE_COMMAND} -P ${PROJECT_SOURCE_DIR}/scripts/check_catch2_string_view.cmake
    RESULT_VARIABLE _catch2_guard_result
    OUTPUT_VARIABLE _catch2_guard_output
    ERROR_VARIABLE _catch2_guard_output
)
if(NOT _catch2_guard_result EQUAL 0)
    message(FATAL_ERROR "${_catch2_guard_output}")
endif()

add_executable(SequentialAdapterTests
    support/AllocationGuard.cpp
    adapter/HostTransportTests.cpp
    adapter/HostTraceTests.cpp
    adapter/MidiConversionTests.cpp
    adapter/BlockEngineTests.cpp
    adapter/ParameterTests.cpp
    adapter/StateTests.cpp
    ../src/HostTransport.cpp
    ../src/HostTrace.cpp
    ../src/MidiConversion.cpp
    ../src/BlockEngine.cpp
    ../src/ParameterIds.cpp
    ../src/ParameterLayout.cpp
    ../src/ParameterReader.cpp
    ../src/PatternStore.cpp
    ../src/StateMigration.cpp
    ../src/StateSerializer.cpp
    ../src/LaneEditHistory.cpp
    ../src/PresetManager.cpp
    ../src/editor/LaneControlStrip.cpp
)

target_include_directories(SequentialAdapterTests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

target_link_libraries(SequentialAdapterTests PRIVATE
    SequentialCoreLib
    Catch2::Catch2WithMain
    juce::juce_audio_basics
    juce::juce_audio_processors
)

set_target_properties(SequentialAdapterTests PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# Enable ctest discovery for Catch2
include(Catch)
catch_discover_tests(SequentialAdapterTests)

# Catch2 string_view linker guard (cpp.instructions.md item 11): runs at configure time so it fails before any compile.
# Not a CTest test: CTest reported "Not Run" for it on macOS runners.
execute_process(
    COMMAND ${CMAKE_COMMAND} -P ${PROJECT_SOURCE_DIR}/scripts/check_catch2_string_view.cmake
    RESULT_VARIABLE _catch2_guard_result
    OUTPUT_VARIABLE _catch2_guard_output
    ERROR_VARIABLE _catch2_guard_output
)
if(NOT _catch2_guard_result EQUAL 0)
    message(FATAL_ERROR "${_catch2_guard_output}")
endif()
