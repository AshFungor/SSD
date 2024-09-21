cmake_minimum_required(VERSION 3.15)

set(SOURCES config-loader.cpp)
set(HEADERS config-loader.hpp)

add_library(utils STATIC ${SOURCES} ${HEADERS})

target_include_directories(utils PUBLIC ${CMAKE_SOURCE_DIR})

target_link_libraries(utils PUBLIC 
    common
    plog::plog
    nlohmann_json::nlohmann_json
)

add_subdirectory(tests)