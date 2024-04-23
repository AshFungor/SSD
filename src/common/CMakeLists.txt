cmake_minimum_required(VERSION 3.15)

project(common LANGUAGES CXX VERSION 0.0)

set(SOURCES thread-pool.cpp callback-queue.cpp shared-buffer.cpp)
set(HEADERS thread-pool.hpp callback-queue.hpp callback.hpp macros.hpp exceptions.hpp shared-buffer.hpp)

add_library(common STATIC ${HEADERS} ${SOURCES})

target_include_directories(common PUBLIC ${SSD_ROOT})

add_subdirectory(tests)