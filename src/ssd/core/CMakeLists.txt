cmake_minimum_required(VERSION 3.15)

set(SOURCES 
    routing-service.cpp
    session.cpp
    message.cpp
    context.cpp
)

set(HEADERS 
    routing-service.hpp
    session.hpp
    message.hpp
    context.hpp
)

declare_ssd_target(
    NAME core 
    TYPE STATIC 
    SOURCES ${SOURCES} ${HEADERS}
    DEPS laar::util laar::protos laar::sound
)

add_library(laar::core ALIAS core)

add_subdirectory(tests)
