cmake_minimum_required(VERSION 3.15)

set(SOURCES
    # jobs
    jobs/async-dispatching-job.cpp
    # dispatchers
    dispatchers/bass-router-dispatcher.cpp dispatchers/tube-dispatcher.cpp
    # sound
    audio-handler.cpp read-handle.cpp write-handle.cpp converter.cpp
)

set(HEADERS
    # jobs
    jobs/async-dispatching-job.hpp
    # interfaces
    interfaces/i-audio-handler.hpp interfaces/i-dispatcher.hpp
    # dispatchers
    dispatchers/bass-router-dispatcher.hpp dispatchers/tube-dispatcher.hpp
    # sound
    audio-handler.hpp read-handle.hpp write-handle.hpp converter.hpp
)

add_library(sound STATIC ${SOURCES} ${HEADERS})

# local deps
target_link_libraries(sound PUBLIC laar::util laar::common laar::protos)
# external deps
target_link_libraries(sound PUBLIC rtaudio plog::plog fftw::fftw nlohmann_json::nlohmann_json)
# includes
target_include_directories(sound PUBLIC ${SRC_ROOT})

add_library(laar::sound ALIAS sound)

add_subdirectory(tests)