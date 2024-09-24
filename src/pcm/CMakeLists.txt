cmake_minimum_required(VERSION 3.15)

add_subdirectory(mapped-pulse) 

add_executable(example example.cpp)

target_link_libraries(example PRIVATE AudioFile laar::pcm)