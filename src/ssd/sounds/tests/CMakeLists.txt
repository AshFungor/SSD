cmake_minimum_required(VERSION 3.15)

project(sound-test LANGUAGES CXX VERSION 0.0)

set(SOURCES sample-converter-test.cpp sound-test.cpp dispatchers-test.cpp)

add_executable(sound-test ${SOURCES})

target_link_libraries(sound-test PRIVATE sound)
target_link_libraries(sound-test PRIVATE dispatchers)
target_link_libraries(sound-test PRIVATE GTest::gtest_main)