add_library(SequentialCoreLib STATIC)

target_include_directories(SequentialCoreLib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_sources(SequentialCoreLib PRIVATE
    sequencing/Constants.h
    sequencing/MusicalTime.h
    sequencing/NoteValues.h
    sequencing/StepTiming.h
    sequencing/StepTiming.cpp
    sequencing/SampleConversion.h
    arpeggiator/DisplayScale.h
    arpeggiator/DisplayScale.cpp
)

set_target_properties(SequentialCoreLib PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)
