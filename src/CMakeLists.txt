cmake_minimum_required(VERSION 3.15)

set(SSD_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory(common)
add_subdirectory(ssd)
add_subdirectory(pcm)
add_subdirectory(tests)