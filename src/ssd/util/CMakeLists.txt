cmake_minimum_required(VERSION 3.15)

project(server-util LANGUAGES CXX VERSION 0.0)

set(SOURCES config-loader.cpp)
set(HEADERS config-loader.hpp)

add_library(server-util STATIC ${SOURCES} ${HEADERS})

target_include_directories(server-util PUBLIC ${SRC_ROOT})

target_link_libraries(server-util PRIVATE nlohmann_json::nlohmann_json)
target_link_libraries(server-util PRIVATE common)

add_subdirectory(tests)