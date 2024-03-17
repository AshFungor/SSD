cmake_minimum_required(VERSION 3.15)

project(common-test LANGUAGES CXX VERSION 0.0)

set(SOURCES callback-queue-test.cpp thread-pool-test.cpp)

add_executable(common-test ${SOURCES})

target_link_libraries(common-test PRIVATE common)
target_link_libraries(common-test PRIVATE GTest::gtest_main)
