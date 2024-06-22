cmake_minimum_required (VERSION 3.8)

file(GLOB_RECURSE slotmaplib_SRC
    "src/*.h"
    "src/*.inl"
    "src/*.cpp")

add_library (slotmaplib INTERFACE ${slotmaplib_SRC})

#target_include_directories (slotmaplib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
