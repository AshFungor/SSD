cmake_minimum_required(VERSION 3.15)

project(contrib LANGUAGES CXX C VERSION 0.0)

add_subdirectory(sockpp)
add_subdirectory(rtaudio)
add_subdirectory(audio-file)

target_link_libraries(sockpp PUBLIC Boost::boost)