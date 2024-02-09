cmake_minimum_required(VERSION 3.15)

project(trace LANGUAGES CXX VERSION 0.0)

set(SOURCES trace.cpp)
set(HEADERS trace.hpp)

add_library(trace STATIC ${HEADERS} ${SOURCES})

target_compile_definitions(trace PUBLIC LOG_TO_STD)
target_compile_definitions(trace PUBLIC LOG_LEVEL=0)

target_link_libraries(trace PRIVATE plog::plog)
target_link_libraries(trace PRIVATE pulse-headers)