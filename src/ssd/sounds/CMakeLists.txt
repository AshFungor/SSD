cmake_minimum_required(VERSION 3.15)

project(sound LANGUAGES CXX VERSION 0.0)

set(SOURCES audio-handler.cpp)
set(HEADERS audio-handler.hpp)

add_library(sound STATIC ${SOURCES} ${HEADERS})

target_include_directories(sound PUBLIC ${SRC_ROOT})

add_subdirectory(interfaces)

target_link_libraries(sound PUBLIC sound-interfaces)
target_link_libraries(sound PUBLIC plog::plog)
target_link_libraries(sound PUBLIC protos)
target_link_libraries(sound PUBLIC common)