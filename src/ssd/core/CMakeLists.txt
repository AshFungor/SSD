cmake_minimum_required(VERSION 3.15)

set(SOURCES 
    routing-service.cpp
)

set(HEADERS 
    routing-service.hpp
)

add_library(core STATIC ${SOURCES} ${HEADERS})

target_include_directories(core PUBLIC ${CMAKE_SOURCE_DIR})

target_link_libraries(core PUBLIC 
    utils
    protos
    sound
)
