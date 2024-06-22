cmake_minimum_required (VERSION 3.8)

file(GLOB_RECURSE slotmap_tests_SRC
    "*.h"
    "*.inl"
    "*.cpp")

add_executable(slotmap_tests ${slotmap_tests_SRC})

target_include_directories (slotmap_tests PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)

target_link_libraries(
    slotmap_tests 
    slotmaplib
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(slotmap_tests)
