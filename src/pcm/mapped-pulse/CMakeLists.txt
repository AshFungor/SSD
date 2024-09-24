cmake_minimum_required(VERSION 3.15)

set(SOURCES simple.cpp trace/trace.cpp)
set(HEADERS simple.hpp trace/trace.hpp)

add_library(pcm STATIC ${HEADERS} ${SOURCES})

target_compile_definitions(pcm PUBLIC LOG_TO_STD)
target_compile_definitions(pcm PUBLIC LOG_LEVEL=0)

target_link_libraries(pcm PUBLIC protos)
target_include_directories(pcm PUBLIC ${CMAKE_SOURCE_DIR})

add_library(laar::pcm ALIAS pcm)
