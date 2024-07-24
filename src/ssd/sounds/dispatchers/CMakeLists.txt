cmake_minimum_required(VERSION 3.15)

project(sound LANGUAGES CXX VERSION 0.0)

set(SOURCES bass-router-dispatcher.cpp tube-dispatcher.cpp)
set(HEADERS bass-router-dispatcher.hpp tube-dispatcher.hpp)

add_library(dispatchers STATIC ${SOURCES} ${HEADERS})

target_include_directories(dispatchers PUBLIC ${SRC_ROOT})

target_link_libraries(dispatchers PUBLIC rtaudio)
target_link_libraries(dispatchers PUBLIC server-util)
target_link_libraries(dispatchers PUBLIC common)
target_link_libraries(dispatchers PUBLIC protos)