cmake_minimum_required(VERSION 3.15)

add_executable(common-test callback-queue-test.cpp thread-pool-test.cpp ring-buffer-test.cpp plain-buffer-test.cpp)

target_link_libraries(common-test PRIVATE laar::common GTest::gtest_main)
