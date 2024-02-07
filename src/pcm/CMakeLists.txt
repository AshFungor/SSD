cmake_minimum_required(VERSION 3.5)

project(pulse-calls-mapper LANGUAGES CXX VERSION 0.0)

set(SOURCES main.cpp)
set(HEADERS )

add_subdirectory(protos)

add_executable(pulse-calls-mapper ${HEADERS} ${SOURCES})

target_link_libraries(pulse-calls-mapper PRIVATE protos)