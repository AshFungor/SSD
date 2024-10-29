cmake_minimum_required(VERSION 3.15)

declare_ssd_test(
    TEST_NAME util-test 
    SOURCES config-loader-test.cpp
    DEPS laar::util
)

target_compile_definitions(util-test PRIVATE BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}/")