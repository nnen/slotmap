cmake_minimum_required (VERSION 3.8)

file(GLOB_RECURSE slotmap_benchmark_SRC
    "*.h"
    "*.inl"
    "*.cpp")

add_executable(slotmap_benchmark ${slotmap_benchmark_SRC})

target_include_directories (slotmap_benchmark PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)

target_link_libraries(
    slotmap_benchmark 
    slotmaplib
    benchmark::benchmark
)

