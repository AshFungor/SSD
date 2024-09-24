cmake_minimum_required(VERSION 3.15)

set(SOURCES 
    thread-pool.cpp 
    callback-queue.cpp 
    ring-buffer.cpp 
    plain-buffer.cpp
)

set(HEADERS 
    thread-pool.hpp 
    callback-queue.hpp 
    callback.hpp 
    macros.hpp 
    exceptions.hpp 
    ring-buffer.hpp 
    plain-buffer.hpp
)

add_library(common STATIC ${HEADERS} ${SOURCES})

target_include_directories(common PUBLIC ${CMAKE_SOURCE_DIR})

add_library(laar::common ALIAS common)

add_subdirectory(tests)