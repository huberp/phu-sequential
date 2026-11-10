add_library(SequentialCoreLib STATIC)

target_include_directories(SequentialCoreLib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_sources(SequentialCoreLib PRIVATE
    sequencing/Constants.h
    sequencing/Constants.cpp
)

set_target_properties(SequentialCoreLib PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)
