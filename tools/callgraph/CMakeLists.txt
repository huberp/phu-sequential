cmake_minimum_required(VERSION 3.15)
project(callgraph LANGUAGES CXX C)

# Find LLVM/Clang - try multiple approaches
find_package(LLVM 17 CONFIG)
find_package(Clang 17 CONFIG)

if(NOT LLVM_FOUND)
  # Try with version suffix
  find_package(LLVM 17.0 CONFIG)
  find_package(Clang 17.0 CONFIG)
endif()

if(NOT LLVM_FOUND)
  message(FATAL_ERROR "LLVM/Clang 17 not found. Install with: sudo apt install llvm-17 clang-17 libclang-17-dev")
endif()

include_directories(
  ${LLVM_INCLUDE_DIRS}
  ${CLANG_INCLUDE_DIRS}
)

add_definitions(${LLVM_DEFINITIONS})

# Add the callgraph tool
add_executable(callgraph callgraph.cpp)

target_link_libraries(callgraph
  PRIVATE
  clangTooling
  clangBasic
  clangAST
  clangLex
  clangParse
  clangSema
  LLVMOption
  LLVMSupport
)
