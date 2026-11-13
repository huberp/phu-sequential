add_executable(SequentialCoreTests
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
