cmake_minimum_required(VERSION 3.15)

project(pulse-calls-mapper LANGUAGES CXX VERSION 0.0)

set(SOURCES main.cpp)
set(HEADERS )

set(PROTOS_SOURCE_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(PROTOS_BINARY_ROOT "${CMAKE_CURRENT_BINARY_DIR}/protos")

add_subdirectory(protos)
add_subdirectory(trace)

add_executable(pulse-calls-mapper ${HEADERS} ${SOURCES})

target_link_libraries(pulse-calls-mapper PRIVATE protos)
target_link_libraries(pulse-calls-mapper PRIVATE sockpp)