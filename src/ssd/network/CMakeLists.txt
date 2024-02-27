cmake_minimum_required(VERSION 3.15)

project(server LANGUAGES CXX VERSION 0.0)

set(SOURCES server.cpp)
set(HEADERS server.hpp)

add_library(server STATIC ${SOURCES} ${HEADERS})

target_include_directories(server PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(server PRIVATE cppserver::cppserver)
