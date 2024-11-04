cmake_minimum_required(VERSION 3.15)

set(SOURCES 
    server.cpp
    message.cpp
    session/context.cpp
    session/stream.cpp
    session/volume.cpp
)

set(HEADERS 
    server.hpp
    message.hpp
    session/context.hpp
    session/stream.hpp
    session/volume.hpp
)

declare_ssd_target(
    NAME core 
    TYPE STATIC 
    SOURCES ${SOURCES} ${HEADERS}
    DEPS laar::util laar::protos laar::sound
)

add_library(laar::core ALIAS core)

add_subdirectory(tests)
