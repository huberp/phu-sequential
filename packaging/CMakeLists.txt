cmake_minimum_required(VERSION 3.31.6)
project(phu-installer LANGUAGES NONE)

if(NOT CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows" OR
   NOT CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "^(AMD64|amd64|x86_64)$" OR
   NOT CMAKE_GENERATOR_PLATFORM STREQUAL "x64")
    message(FATAL_ERROR "The Windows x64 installer requires a native Windows x64 host.")
endif()

set(PHU_PLUGIN_BUILD "" CACHE PATH "Configured application build directory")
set(PHU_RELEASE_TAG "" CACHE STRING "Release version, e.g. v0.1.0 or v0.1.0-rc1")
set(PHU_REDIST "" CACHE FILEPATH "Verified Microsoft VC++ x64 redistributable")
set(PHU_PRODUCT "phu-sequential")
set(PHU_PUBLISHER "PHU")

if(NOT PHU_RELEASE_TAG MATCHES "^v(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)(-rc(0|[1-9][0-9]*))?$")
    message(FATAL_ERROR "PHU_RELEASE_TAG must be vM.m.p or vM.m.p-rcN.")
endif()
set(major "${CMAKE_MATCH_1}")
set(minor "${CMAKE_MATCH_2}")
set(patch "${CMAKE_MATCH_3}")
set(rc "${CMAKE_MATCH_5}")
if(major GREATER 255 OR minor GREATER 255 OR patch GREATER 64)
    message(FATAL_ERROR "Release version exceeds the MSI version mapping limits.")
endif()
if(rc STREQUAL "")
    set(ordinal 999)
elseif(rc LESS 1 OR rc GREATER 998)
    message(FATAL_ERROR "Release candidate number must be between 1 and 998.")
else()
    set(ordinal "${rc}")
endif()
math(EXPR msi_build "${patch} * 1000 + ${ordinal}")
set(msi_version "${major}.${minor}.${msi_build}")

find_program(WIX_EXECUTABLE NAMES wix.exe wix REQUIRED)
find_program(POWERSHELL_EXECUTABLE NAMES powershell.exe REQUIRED)
execute_process(COMMAND "${WIX_EXECUTABLE}" --version
    OUTPUT_VARIABLE wix_version OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE wix_result)
if(NOT wix_result EQUAL 0 OR NOT wix_version MATCHES "^7\\.0\\.1(\\+|$)")
    message(FATAL_ERROR "WiX 7.0.1 is required; found '${wix_version}'.")
endif()
set(bundle "${PHU_PLUGIN_BUILD}/src/phu-sequential_artefacts/Release/VST3/${PHU_PRODUCT}.vst3")
set(msi "${CMAKE_CURRENT_BINARY_DIR}/${PHU_PRODUCT}.msi")
set(output "${CMAKE_CURRENT_BINARY_DIR}/${PHU_PRODUCT}-${PHU_RELEASE_TAG}-windows-x64.exe")

add_custom_target(installer ALL
    COMMAND "${POWERSHELL_EXECUTABLE}" -NoProfile -ExecutionPolicy Bypass
        -File "${CMAKE_CURRENT_SOURCE_DIR}/windows/VerifyInputs.ps1"
        -BuildDirectory "${PHU_PLUGIN_BUILD}" -Bundle "${bundle}"
        -Redist "${PHU_REDIST}" -Product "${PHU_PRODUCT}"
        -PluginVersion "${major}.${minor}.${patch}"
        -OutputFile "${CMAKE_CURRENT_BINARY_DIR}/runtime-version.txt"
    COMMAND "${CMAKE_COMMAND}"
        "-DWIX_EXECUTABLE=${WIX_EXECUTABLE}"
        "-DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}"
        "-DBUNDLE=${bundle}"
        "-DREDIST=${PHU_REDIST}"
        "-DPRODUCT=${PHU_PRODUCT}"
        "-DPUBLISHER=${PHU_PUBLISHER}"
        "-DTAG=${PHU_RELEASE_TAG}"
        "-DVERSION=${msi_version}"
        "-DMSI=${msi}"
        "-DOUTPUT=${output}"
        "-DBUILD_DIR=${CMAKE_CURRENT_BINARY_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/windows/Package.cmake"
    VERBATIM)
