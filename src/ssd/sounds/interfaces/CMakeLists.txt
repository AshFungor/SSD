cmake_minimum_required(VERSION 3.15)

project(sound LANGUAGES CXX VERSION 0.0)

set(HEADERS i-message.hpp i-protocol.hpp)

add_library(sound-interfaces INTERFACE)

target_include_directories(sound-interfaces INTERFACE ${SRC_ROOT})