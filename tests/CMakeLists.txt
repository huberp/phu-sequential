add_executable(SequentialCoreTests
    core/PlaceholderTests.cpp
    core/ConstantsTests.cpp
    core/MusicalTimeTests.cpp
    core/NoteValuesTests.cpp
    core/StepTimingTests.cpp
    core/SampleConversionTests.cpp
    core/DisplayScaleTests.cpp
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
