cmake_minimum_required(VERSION 3.15)

declare_ssd_test(
    TEST_NAME pcm-test
    SOURCES mainloop-test.cpp context-test.cpp stream-test.cpp
    DEPS laar::pcm
)