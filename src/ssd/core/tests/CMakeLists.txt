cmake_minimum_required(VERSION 3.15)

declare_ssd_test(
    TEST_NAME core-test
    SOURCES message-test.cpp 
    DEPS laar::core
)