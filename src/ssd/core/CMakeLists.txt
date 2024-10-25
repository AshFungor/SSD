cmake_minimum_required(VERSION 3.15)

set(SOURCES 
    routing-service.cpp
    session.cpp
    header.cpp
)

set(HEADERS 
    routing-service.hpp
    session.hpp
    header.hpp
)

add_library(core STATIC ${SOURCES} ${HEADERS})

target_include_directories(core PUBLIC ${CMAKE_SOURCE_DIR})
target_link_libraries(core PUBLIC 
    ${SSD_EXTERNAL_DEPS} 
    laar::util 
    laar::protos 
    laar::sound
)

add_library(laar::core ALIAS core)
