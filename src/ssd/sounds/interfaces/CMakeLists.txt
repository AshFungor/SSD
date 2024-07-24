cmake_minimum_required(VERSION 3.15)

project(sound LANGUAGES CXX VERSION 0.0)

set(HEADERS 
    i-audio-handler.hpp
    i-dispatcher.hpp
)

add_library(sound-interfaces INTERFACE ${HEADERS})

target_include_directories(sound-interfaces INTERFACE ${SRC_ROOT})

target_link_libraries(sound-interfaces INTERFACE GTest::gtest_main)