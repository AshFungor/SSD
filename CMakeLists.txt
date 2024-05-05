cmake_minimum_required(VERSION 3.15)

project(sound-server LANGUAGES CXX C VERSION 0.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_EXTENSIONS ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# packages
find_package(protobuf CONFIG REQUIRED VERSION 3.21.12)
find_package(nlohmann_json REQUIRED VERSION 3.11.3)
find_package(GTest REQUIRED VERSION 1.14.0)
find_package(plog REQUIRED VERSION 1.1.10)
find_package(Boost REQUIRED VERSION 1.84.0)

set(ROOT ${CMAKE_SOURCE_DIR})

# contrib libs not on conan
add_subdirectory(contrib)

# dummy target containing headers
add_subdirectory(pulse)

add_subdirectory(src)
