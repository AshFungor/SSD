cmake_minimum_required(VERSION 3.15)

set(SOURCES
    # buffer (inherited from common)
    ring-buffer.cpp
    # dispatchers
    dispatchers/bass-router-dispatcher.cpp dispatchers/tube-dispatcher.cpp
    # sound
    audio-handler.cpp read-handle.cpp write-handle.cpp converter.cpp
)

set(HEADERS
    # buffer (inherited from common)
    ring-buffer.hpp
    # interfaces
    interfaces/i-audio-handler.hpp interfaces/i-dispatcher.hpp
    # dispatchers
    dispatchers/bass-router-dispatcher.hpp dispatchers/tube-dispatcher.hpp
    # sound
    audio-handler.hpp read-handle.hpp write-handle.hpp converter.hpp
)

declare_ssd_target(
    NAME sound 
    TYPE STATIC 
    SOURCES ${SOURCES} 
    HEADERS ${HEADERS} 
    LOCAL_DEPS laar::util laar::protos rtaudio
)

add_library(laar::sound ALIAS sound)

add_subdirectory(tests)