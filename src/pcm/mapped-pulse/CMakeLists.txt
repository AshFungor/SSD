cmake_minimum_required(VERSION 3.15)

set(SOURCES 
    simple.cpp 
    context.cpp
    mainloop/mainloop.cpp
    mainloop/threaded-mainloop.cpp
    trace/trace.cpp 
    utils/utf8.cpp
    utils/version.cpp
    utils/xmalloc.cpp
    utils/channel-map.cpp
)
set(HEADERS 
    simple.hpp 
    trace/trace.hpp 
)

declare_ssd_target(
    NAME pcm 
    TYPE STATIC 
    SOURCES ${SOURCES} ${HEADERS}
    DEPS laar::sound laar::core
)

add_library(laar::pcm ALIAS pcm)

add_subdirectory(tests)
