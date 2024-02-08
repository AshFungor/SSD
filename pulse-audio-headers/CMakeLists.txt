cmake_minimum_required(VERSION 3.15)

project(pulse-headers LANGUAGES C VERSION 17.0)

add_library(pulse-headers INTERFACE)

target_include_directories(pulse-headers INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(pulse-headers INTERFACE ${glib_INCLUDE_DIR})