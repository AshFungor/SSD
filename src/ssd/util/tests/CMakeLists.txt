cmake_minimum_required(VERSION 3.15)

project(util-test LANGUAGES CXX VERSION 0.0)

set(SOURCES config-loader-test.cpp)

add_executable(util-test ${SOURCES})

target_compile_definitions(util-test PRIVATE BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}/")

target_link_libraries(util-test PRIVATE common)
target_link_libraries(util-test PRIVATE server-util)
target_link_libraries(util-test PRIVATE GTest::gtest_main)
target_link_libraries(util-test PRIVATE nlohmann_json::nlohmann_json)
