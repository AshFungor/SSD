cmake_minimum_required(VERSION 3.15)

project(contrib LANGUAGES CXX C VERSION 0.0)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build rtaudio shared library" FORCE)

add_subdirectory(rtaudio)
add_subdirectory(audio-file)

target_link_libraries(sockpp-static PUBLIC Boost::boost)
add_library(sockpp ALIAS sockpp-static)