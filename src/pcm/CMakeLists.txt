cmake_minimum_required(VERSION 3.15)

add_subdirectory(mapped-pulse)

declare_ssd_target(
    NAME example
    TYPE EXECUTABLE 
    SOURCES example.cpp
    DEPS AudioFile laar::pcm
)