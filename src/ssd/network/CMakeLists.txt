cmake_minimum_required(VERSION 3.15)

project(server LANGUAGES CXX VERSION 0.0)

set(SOURCES server.cpp session.cpp message.cpp)
set(HEADERS server.hpp session.hpp message.hpp)

add_library(server STATIC ${SOURCES} ${HEADERS})

target_include_directories(server PUBLIC ${SRC_ROOT})

target_link_libraries(server PUBLIC server-util)
target_link_libraries(server PUBLIC tiny-fsm)
target_link_libraries(server PUBLIC sockpp)
target_link_libraries(server PUBLIC protos)
