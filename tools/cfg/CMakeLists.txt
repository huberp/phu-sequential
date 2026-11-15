cmake_minimum_required(VERSION 3.15)
project(cfg LANGUAGES CXX C)

# Disable optional LLVM dependencies that may not be installed
set(LLVM_ENABLE_FFI OFF CACHE BOOL "Disable FFI")
set(LLVM_ENABLE_TERMINFO OFF CACHE BOOL "Disable Terminfo")
set(LLVM_ENABLE_LIBEDIT OFF CACHE BOOL "Disable LibEdit")

find_package(LLVM 17 CONFIG REQUIRED)
find_package(Clang 17 CONFIG REQUIRED)
include_directories(${LLVM_INCLUDE_DIRS} ${CLANG_INCLUDE_DIRS})
add_executable(cfg cfg.cpp)
target_link_libraries(cfg PRIVATE clangTooling clangBasic clangAST clangLex clangParse clangSema LLVMOption LLVMSupport)
