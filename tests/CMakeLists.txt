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

# Catch2 string_view linker guard (cpp.instructions.md item 11): fails if any assertion stringifies a std::string_view.
# Pure CMake script so it runs on every CI platform without a Python dependency.
add_test(NAME Catch2StringViewGuard
    COMMAND ${CMAKE_COMMAND} -P ${PROJECT_SOURCE_DIR}/scripts/check_catch2_string_view.cmake)
