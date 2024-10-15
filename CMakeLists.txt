cmake_minimum_required(VERSION 3.12)
project(example LANGUAGES C CXX)

# Ensure that the user has set LLVM_PATH to the directory
# containing the LLVM build they want to use.
if (NOT DEFINED LLVM_PATH)
    message(FATAL_ERROR "Please set LLVM_PATH to the directory where you built LLVM")
endif()

# Find LLVM and Clang.
set(LLVM_DIR "${LLVM_PATH}/lib/cmake/llvm")
set(Clang_DIR "${LLVM_PATH}/lib/cmake/clang")
find_package(LLVM REQUIRED CONFIG NO_DEFAULT_PATH)
find_package(Clang REQUIRED CONFIG NO_DEFAULT_PATH)

# Print the package version so we can tell at a glance
# if we actually did this right.
message(STATUS "Found LLVM ${LLVM_VERSION}")

# Include the required LLVM and Clang CMake modules.
list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
list(APPEND CMAKE_MODULE_PATH "${CLANG_CMAKE_DIR}")
include(AddLLVM)
include(AddClang)

# Define our executable.
add_clang_executable(example
    main.cc
)

# Add Clang's and LLVM's include directories.
target_include_directories(example SYSTEM PRIVATE
    ${LLVM_INCLUDE_DIRS}
    ${CLANG_INCLUDE_DIRS}
)

# Link against any libraries we need.
target_link_libraries(example PRIVATE
    clangAST
    clangFrontend
    clangTooling
)

# Tell our program where clang is.
target_compile_definitions(example PRIVATE
    "CLANG_PATH=\"${LLVM_PATH}/bin/clang++\""
)
