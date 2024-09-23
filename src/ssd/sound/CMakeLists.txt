cmake_minimum_required(VERSION 3.15)

set(SOURCES 
    audio-handler.cpp 
    read-handle.cpp 
    write-handle.cpp 
    converter.cpp
)

set(HEADERS 
    audio-handler.hpp 
    read-handle.hpp 
    write-handle.hpp 
    converter.hpp
)

add_library(sound STATIC ${SOURCES} ${HEADERS})

target_include_directories(sound PUBLIC ${SRC_ROOT})

add_subdirectory(interfaces)
add_subdirectory(dispatchers)
add_subdirectory(jobs)
add_subdirectory(tests)

target_link_libraries(sound PUBLIC 
    jobs
    common
    protos
    rtaudio
    plog::plog
    dispatchers
    server-util
    nlohmann_json::nlohmann_json
)
