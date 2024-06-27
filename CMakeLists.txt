cmake_minimum_required (VERSION 3.8)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

project("slotmap")

include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    googlebenchmark
    GIT_REPOSITORY "https://github.com/google/benchmark.git"
    GIT_TAG main
    PATCH_COMMAND ""
)
FetchContent_MakeAvailable(googlebenchmark)
FetchContent_GetProperties(benchmark SOURCE_DIR BENCHMARK_INCLUDE_DIR)
include_directories(${BENCHMARK_INCLUDE_DIR})

add_subdirectory("slotmap")
add_subdirectory("slotmap-tests")
add_subdirectory("slotmap-benchmark")

#execute_process(
#    COMMAND ${CMAKE_COMMAND} -E copy_if_different
#        ${CMAKE_BINARY_DIR}/compile_commands.json
#        ${CMAKE_CURRENT_LIST_DIR}
#    COMMAND_ECHO STDERR
#)
