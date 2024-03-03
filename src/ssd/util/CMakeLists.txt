cmake_minimum_required(VERSION 3.15)

project(server-util LANGUAGES CXX VERSION 0.0)

set(SOURCES thread-pool.cpp callback-queue.cpp config-loader.cpp)
set(HEADERS thread-pool.hpp callback-queue.hpp config-loader.hpp)

add_library(server-util STATIC ${SOURCES} ${HEADERS})

target_include_directories(server-util PUBLIC ${SRC_ROOT})
target_link_libraries(server-util PRIVATE nlohmann_json::nlohmann_json)

add_subdirectory(tests)