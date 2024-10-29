cmake_minimum_required(VERSION 3.15)

add_subdirectory(util)
add_subdirectory(core)
add_subdirectory(sound)

declare_ssd_target(
    NAME sound-server-daemon 
    TYPE EXECUTABLE 
    SOURCES main.cpp
    LOCAL_DEPS laar::core laar::sound laar::util abseil::abseil
)
