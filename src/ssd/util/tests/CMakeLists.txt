cmake_minimum_required(VERSION 3.15)

add_executable(util-test config-loader-test.cpp)

target_compile_definitions(util-test PRIVATE BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}/")

target_link_libraries(util-test PRIVATE laar::util GTest::gtest_main)
