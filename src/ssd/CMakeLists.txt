cmake_minimum_required(VERSION 3.15)

project(sound-server-daemon LANGUAGES CXX VERSION 0.0)

set(SOURCES main.cpp)
set(HEADERS )

add_executable(sound-server-daemon ${HEADERS} ${SOURCES})

set(SRC_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory(util)
add_subdirectory(sounds)
add_subdirectory(network)

target_link_libraries(sound-server-daemon PRIVATE server)
target_link_libraries(sound-server-daemon PRIVATE sound)
