cmake_minimum_required(VERSION 3.15)

declare_ssd_target(
    NAME util-test 
    TYPE EXECUTABLE
    SOURCES config-loader-test.cpp
    LOCAL_DEPS laar::util
    TEST ON
)

target_compile_definitions(util-test PRIVATE BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}/")