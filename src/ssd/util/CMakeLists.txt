cmake_minimum_required(VERSION 3.15)

set(SOURCES config-loader.cpp)
set(HEADERS config-loader.hpp)

add_library(util STATIC ${SOURCES} ${HEADERS})

target_include_directories(util PUBLIC ${CMAKE_SOURCE_DIR})
target_link_libraries(util PUBLIC laar::common plog::plog nlohmann_json::nlohmann_json)

add_library(laar::util ALIAS util)

add_subdirectory(tests)