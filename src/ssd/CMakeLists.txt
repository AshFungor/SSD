cmake_minimum_required(VERSION 3.15)

add_executable(sound-server-daemon main.cpp)

add_subdirectory(util)
add_subdirectory(core)
add_subdirectory(sound)

target_link_libraries(sound-server-daemon PRIVATE laar::core laar::sound laar::util abseil::abseil)
