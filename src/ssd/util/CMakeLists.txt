cmake_minimum_required(VERSION 3.15)

set(SOURCES config-loader.cpp)
set(HEADERS config-loader.hpp)

declare_ssd_target(
    NAME util 
    TYPE STATIC 
    SOURCES ${SOURCES} ${HEADERS}
)

add_library(laar::util ALIAS util)

add_subdirectory(tests)