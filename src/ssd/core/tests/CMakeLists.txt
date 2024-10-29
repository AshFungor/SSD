cmake_minimum_required(VERSION 3.15)

declare_ssd_target(
    NAME core-test
    TYPE EXECUTABLE
    SOURCES message-test.cpp 
    LOCAL_DEPS laar::core
    TEST ON
)