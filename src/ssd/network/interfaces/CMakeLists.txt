cmake_minimum_required(VERSION 3.15)

project(server LANGUAGES CXX VERSION 0.0)

set(HEADERS i-message.hpp i-protocol.hpp)

add_library(interfaces INTERFACE ${HEADERS})

target_include_directories(interfaces INTERFACE ${SRC_ROOT})