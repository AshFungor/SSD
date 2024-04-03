cmake_minimum_required(VERSION 3.15)

project(server LANGUAGES CXX VERSION 0.0)

set(SOURCES protocol.cpp)
set(HEADERS protocol.hpp)

add_library(protocol-simple STATIC ${SOURCES} ${HEADERS})

target_include_directories(protocol-simple PUBLIC ${SRC_ROOT})

target_link_libraries(protocol-simple PUBLIC msfsm)
target_link_libraries(protocol-simple PUBLIC protos)
target_link_libraries(protocol-simple PUBLIC common)