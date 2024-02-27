cmake_minimum_required(VERSION 3.15)

project(pulse-headers LANGUAGES C VERSION 17.0)

add_library(pulse-headers INTERFACE)

target_include_directories(pulse-headers INTERFACE ${ROOT})
target_include_directories(pulse-headers INTERFACE ${glib_INCLUDE_DIR})
