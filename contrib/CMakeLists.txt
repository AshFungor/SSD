cmake_minimum_required(VERSION 3.15)

project(contrib LANGUAGES CXX C VERSION 0.0)

set(SOCKPP_BUILD_SHARED OFF CACHE BOOL "Build sockpp shared library" FORCE)
set(SOCKPP_BUILD_STATIC ON CACHE BOOL "Build sockpp static library" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build rtaudio shared library" FORCE)

add_subdirectory(sockpp)
add_subdirectory(rtaudio)
add_subdirectory(audio-file)

target_link_libraries(sockpp-static PUBLIC Boost::boost)
add_library(sockpp ALIAS sockpp-static)