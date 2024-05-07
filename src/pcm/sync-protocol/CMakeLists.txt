cmake_minimum_required(VERSION 3.15)

project(pulse-calls-mapper LANGUAGES CXX VERSION 0.0)

set(SOURCES sync-protocol.cpp)
set(HEADERS sync-protocol.hpp)

add_library(sync-protocol STATIC ${HEADERS} ${SOURCES})

target_link_libraries(sync-protocol PUBLIC protos)
target_link_libraries(sync-protocol PUBLIC sockpp)
target_link_libraries(sync-protocol PUBLIC trace)
target_link_libraries(sync-protocol PUBLIC pulse-headers)
target_link_libraries(sync-protocol PUBLIC server)
