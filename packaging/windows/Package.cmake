file(READ "${BUILD_DIR}/runtime-version.txt" runtime_version)
string(REPLACE "." ";" runtime_parts "${runtime_version}")
list(GET runtime_parts 1 runtime_minor)
list(GET runtime_parts 2 runtime_build)
list(GET runtime_parts 3 runtime_revision)
configure_file("${SOURCE_DIR}/windows/Theme.xml" "${BUILD_DIR}/Theme.xml" @ONLY)
execute_process(COMMAND "${WIX_EXECUTABLE}" build -arch x64
    -d "Product=${PRODUCT}" -d "Publisher=${PUBLISHER}" -d "Version=${VERSION}"
    -d "RuntimeVersion=${runtime_version}"
    -d "Bundle=${BUNDLE}" -o "${MSI}" "${SOURCE_DIR}/windows/Plugin.wxs"
    RESULT_VARIABLE result)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "WiX MSI build failed (${result}).")
endif()
execute_process(COMMAND "${WIX_EXECUTABLE}" build -arch x64
    -ext WixToolset.BootstrapperApplications.wixext/7.0.1 -ext WixToolset.Util.wixext/7.0.1
    -d "Product=${PRODUCT}" -d "Publisher=${PUBLISHER}" -d "Version=${VERSION}"
    -d "Tag=${TAG}" -d "RuntimeMinor=${runtime_minor}" -d "RuntimeBuild=${runtime_build}"
    -d "RuntimeRevision=${runtime_revision}" -d "Redist=${REDIST}"
    -d "Msi=${MSI}" -d "Theme=${BUILD_DIR}/Theme.xml"
    -o "${OUTPUT}" "${SOURCE_DIR}/windows/Bundle.wxs"
    RESULT_VARIABLE result)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "WiX bundle build failed (${result}).")
endif()
message(STATUS "Installer: ${OUTPUT}")
