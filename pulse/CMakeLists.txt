cmake_minimum_required(VERSION 3.19)

project(pulse-headers LANGUAGES C VERSION 17.0)

add_library(pulse-headers INTERFACE)

target_include_directories(pulse-headers INTERFACE ${CMAKE_SOURCE_DIR})
