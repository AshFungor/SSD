cmake_minimum_required(VERSION 3.15)

project(example-sound LANGUAGES CXX VERSION 0.0)

set(HEADERS )
set(SOURCES pcm.c)

add_executable(example ${SOURCES} ${HEADERS})

target_link_libraries(example PUBLIC ALSA::ALSA)
target_link_libraries(example PUBLIC plog::plog)