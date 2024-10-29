cmake_minimum_required(VERSION 3.15)

declare_ssd_test(
    TEST_NAME sound-test 
    SOURCES dispatchers-test.cpp sample-converter-test.cpp sound-test.cpp
    DEPS laar::sound
)
