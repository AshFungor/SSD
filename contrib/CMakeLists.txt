cmake_minimum_required(VERSION 3.15)

project(contrib LANGUAGES CXX C VERSION 0.0)

# RtAudio
set(RTAUDIO_BUILD_TESTING OFF CACHE BOOL "Build rtaudio tests" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build rtaudio shared library" FORCE)

set(BUILD_TESTS OFF CACHE BOOL "Build AudioFile tests" FORCE)

add_subdirectory(rtaudio)
add_subdirectory(AudioFile)