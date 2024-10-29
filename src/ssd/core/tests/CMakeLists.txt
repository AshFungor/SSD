cmake_minimum_required(VERSION 3.15)

add_executable(core-test message-test.cpp)

target_link_libraries(core-test PRIVATE laar::core GTest::gtest_main)

add_test(NAME core-test COMMAND core-test)
