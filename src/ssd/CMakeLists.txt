cmake_minimum_required(VERSION 3.15)

project(sound-server-daemon LANGUAGES CXX VERSION 0.0)

set(SOURCES main.cpp)
set(HEADERS )

add_executable(sound-server-daemon ${HEADERS} ${SOURCES})

add_subdirectory(util)

target_link_libraries(sound-server-daemon PRIVATE asio::asio)
target_link_libraries(sound-server-daemon PRIVATE server-util)