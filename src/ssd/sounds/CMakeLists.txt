cmake_minimum_required(VERSION 3.15)

project(sound LANGUAGES CXX VERSION 0.0)

set(SOURCES audio-handler.cpp read-handle.cpp write-handle.cpp converter.cpp)
set(HEADERS audio-handler.hpp read-handle.hpp write-handle.hpp converter.hpp)

add_library(sound STATIC ${SOURCES} ${HEADERS})

target_include_directories(sound PUBLIC ${SRC_ROOT})

add_subdirectory(interfaces)
add_subdirectory(tests)

target_link_libraries(sound PUBLIC plog::plog)
target_link_libraries(sound PUBLIC rtaudio)
target_link_libraries(sound PUBLIC protos)
target_link_libraries(sound PUBLIC server-util)
target_link_libraries(sound PUBLIC common)