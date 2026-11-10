cmake_minimum_required(VERSION 3.27.6)
project(phu-sequential VERSION 0.1.0 LANGUAGES C CXX)

# =============================================================================
# Top-Level Guard
# =============================================================================
if(NOT PROJECT_IS_TOP_LEVEL)
    return()
endif()

# =============================================================================
# Fetch Dependencies (reusable helpers in cmake/PhuDependencies.cmake)
# =============================================================================
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(PhuDependencies)
phu_fetch_juce(TAG 9.0.4)
phu_fetch_catch2(TAG v3.5.0)

# =============================================================================
# Global Settings
# =============================================================================
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include(cmake/compiler_flags.cmake)

# =============================================================================
# Platform-Specific Settings
# =============================================================================
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    enable_language(OBJC OBJCXX)
endif()

# =============================================================================
# Standard Settings
# =============================================================================
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# =============================================================================
# Dependency Validation
# =============================================================================
foreach(SUBMODULE IN ITEMS JUCE Catch2)
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${SUBMODULE}/CMakeLists.txt")
        message(FATAL_ERROR
            "${SUBMODULE} dependency not initialized. "
            "Make sure that dependencies JUCE and Catch2 are properly cloned and initialized. "
        )
    endif()
endforeach()

# =============================================================================
# JUCE & Dependencies
# =============================================================================
include(cmake/juce_config.cmake)

enable_testing()

add_subdirectory(lib)
add_subdirectory(src)
add_subdirectory(tests)
