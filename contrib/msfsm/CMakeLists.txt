cmake_minimum_required(VERSION 3.5)

project(msfsm VERSION 0.3.3 LANGUAGES CXX)

set(HEADERS msfsm.hpp)

add_library(msfsm INTERFACE ${HEADERS})

target_include_directories(msfsm INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})