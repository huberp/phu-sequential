# Guard against the Catch2 string_view linker error (see .github/instructions/cpp.instructions.md, item 11).
#
# Catch2 v3 declares Catch::StringMaker<std::string_view>, but on MSVC its convert() is not linked, so any
# assertion that stringifies a std::string_view fails with LNK2019/LNK1120 on Windows.
#
# This script collects every std::string_view data member or variable declared under lib/ and tests/, then
# fails if a CHECK*/REQUIRE*/WARN/INFO/CAPTURE assertion in tests/ uses one of those names without wrapping
# the operand in static_cast<std::string>.
#
# Pure CMake on purpose: it runs identically on Windows, Linux and macOS without a Python dependency.
# Usage: cmake -P scripts/check_catch2_string_view.cmake   (exit code 0 = clean, 1 = violations)

get_filename_component(REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

file(GLOB_RECURSE ALL_FILES "${REPO_ROOT}/lib/*" "${REPO_ROOT}/tests/*")
set(SOURCE_FILES "")
foreach(FILE_PATH IN LISTS ALL_FILES)
    if("${FILE_PATH}" MATCHES "\\.(h|hpp|cpp|cc|cxx)$")
        list(APPEND SOURCE_FILES "${FILE_PATH}")
    endif()
endforeach()

# Pass 1: collect names of std::string_view data members and variables (not parameters).
# Each match is rewritten to a marker so that only names (never ';') end up in a CMake list.
set(SV_MARK "@SV@")
set(NAMES "")
foreach(FILE_PATH IN LISTS SOURCE_FILES)
    file(READ "${FILE_PATH}" TEXT)
    string(REGEX REPLACE
        "std::string_view[ \t\r\n]+([A-Za-z_][A-Za-z0-9_]*)[ \t\r\n]*[;={]"
        "\n${SV_MARK}\\1\n" MARKED "${TEXT}")
    string(REGEX MATCHALL "${SV_MARK}[A-Za-z_][A-Za-z0-9_]*" FOUND "${MARKED}")
    foreach(ENTRY IN LISTS FOUND)
        string(REPLACE "${SV_MARK}" "" NAME "${ENTRY}")
        list(APPEND NAMES "${NAME}")
    endforeach()
endforeach()
list(REMOVE_DUPLICATES NAMES)

# Pass 2: scan assertions in tests/. Semicolons are masked so that every line is a safe list element.
# Assertions can span lines, so parentheses are balanced across lines before the check runs.
set(VIOLATIONS "")
if(NAMES)
    foreach(FILE_PATH IN LISTS SOURCE_FILES)
        file(RELATIVE_PATH REL "${REPO_ROOT}" "${FILE_PATH}")
        if(NOT REL MATCHES "^tests/")
            continue()
        endif()

        file(READ "${FILE_PATH}" TEXT)
        string(REPLACE ";" "<SC>" TEXT "${TEXT}")
        string(REPLACE "\n" ";" LINES "${TEXT}")

        set(IN_ASSERT FALSE)
        set(BUFFER "")
        set(DEPTH 0)
        set(START_LINE 0)
        set(START_TEXT "")
        set(LINE_NO 0)
        foreach(LINE IN LISTS LINES)
            math(EXPR LINE_NO "${LINE_NO} + 1")
            if(NOT IN_ASSERT)
                if("${LINE}" MATCHES "(^|[^A-Za-z0-9_])(CHECK|REQUIRE|WARN|INFO|CAPTURE)[A-Za-z0-9_]*[ \t]*\\(")
                    set(IN_ASSERT TRUE)
                    set(BUFFER "")
                    set(DEPTH 0)
                    set(START_LINE ${LINE_NO})
                    set(START_TEXT "${LINE}")
                endif()
            endif()
            if(IN_ASSERT)
                string(APPEND BUFFER "${LINE}\n")
                string(REGEX MATCHALL "\\(" OPENS "${LINE}")
                string(REGEX MATCHALL "\\)" CLOSES "${LINE}")
                list(LENGTH OPENS OPEN_COUNT)
                list(LENGTH CLOSES CLOSE_COUNT)
                math(EXPR DEPTH "${DEPTH} + ${OPEN_COUNT} - ${CLOSE_COUNT}")
                if(DEPTH LESS 1)
                    set(IN_ASSERT FALSE)
                    string(FIND "${BUFFER}" "static_cast<std::string>" SAFE_POS)
                    if(SAFE_POS EQUAL -1)
                        foreach(NAME IN LISTS NAMES)
                            if("${BUFFER}" MATCHES "(^|[^A-Za-z0-9_])${NAME}([^A-Za-z0-9_]|$)")
                                list(APPEND VIOLATIONS "${REL}:${START_LINE}: ${START_TEXT} (string_view: ${NAME})")
                                break()
                            endif()
                        endforeach()
                    endif()
                endif()
            endif()
        endforeach()
    endforeach()
endif()

list(LENGTH VIOLATIONS VIOLATION_COUNT)
if(VIOLATION_COUNT GREATER 0)
    message("Catch2 string_view guard: assertions stringify std::string_view (breaks the Windows link).")
    message("Fix: wrap the operand, e.g. CHECK(static_cast<std::string>(obj.name) == \"value\").")
    foreach(VIOLATION IN LISTS VIOLATIONS)
        string(REPLACE "<SC>" ";" SHOWN "${VIOLATION}")
        message("  ${SHOWN}")
    endforeach()
    message(FATAL_ERROR "Catch2 string_view guard failed with ${VIOLATION_COUNT} violation(s).")
endif()

message(STATUS "Catch2 string_view guard: OK")
