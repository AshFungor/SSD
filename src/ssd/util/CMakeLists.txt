cmake_minimum_required(VERSION 3.15)

project(server-util LANGUAGES CXX VERSION 0.0)

set(SOURCES thread-pool.cpp)
set(HEADERS thread-pool.hpp)

add_library(server-util STATIC ${SOURCES} ${HEADERS})

target_include_directories(server-util PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})