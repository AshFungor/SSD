cmake_minimum_required(VERSION 3.15)

project(server LANGUAGES CXX VERSION 0.0)

set(SOURCES server.cpp session.cpp message.cpp sync-protocol.cpp)
set(HEADERS server.hpp session.hpp message.hpp sync-protocol.hpp)

add_library(server STATIC ${SOURCES} ${HEADERS})

target_include_directories(server PUBLIC ${SRC_ROOT})

target_link_libraries(server PUBLIC server-util)
target_link_libraries(server PUBLIC sockpp)
target_link_libraries(server PUBLIC protos)

add_subdirectory(interfaces)
add_subdirectory(protocols)

target_link_libraries(server PUBLIC interfaces)
