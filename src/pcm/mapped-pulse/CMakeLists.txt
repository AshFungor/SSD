cmake_minimum_required(VERSION 3.15)

set(SOURCES 
    simple.cpp 
    trace/trace.cpp 
    utils/utf8.cpp 
    utils/xmalloc.cpp
    channel-map/channel-map.cpp
)
set(HEADERS 
    simple.hpp 
    trace/trace.hpp 
    utils/macros.hpp
)

add_library(pcm STATIC ${HEADERS} ${SOURCES})

target_link_libraries(pcm PUBLIC protos ${SSD_EXTERNAL_DEPS})

target_include_directories(pcm PUBLIC ${CMAKE_SOURCE_DIR})

add_library(laar::pcm ALIAS pcm)
