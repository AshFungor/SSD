cmake_minimum_required(VERSION 3.15)

add_executable(sound-test sample-converter-test.cpp sound-test.cpp dispatchers-test.cpp)

target_link_libraries(sound-test PRIVATE laar::sound GTest::gtest_main)

add_test(NAME sound-test COMMAND sound-test)
