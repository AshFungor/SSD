cmake_minimum_required(VERSION 3.15)

add_subdirectory(mapped-pulse)

declare_ssd_target(
    NAME sync-example
    TYPE EXECUTABLE 
    SOURCES sync-example.cpp
    DEPS AudioFile laar::pcm
)

declare_ssd_target(
    NAME async-example
    TYPE EXECUTABLE 
    SOURCES async-example.cpp
    DEPS AudioFile laar::pcm
)