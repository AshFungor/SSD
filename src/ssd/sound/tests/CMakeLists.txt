cmake_minimum_required(VERSION 3.15)

declare_ssd_target(
    NAME sound-test 
    TYPE EXECUTABLE
    SOURCES dispatchers-test.cpp sample-converter-test.cpp sound-test.cpp
    LOCAL_DEPS laar::sound
    TEST ON
)
