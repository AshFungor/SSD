cmake_minimum_required(VERSION 3.15)

project(tests LANGUAGES CXX VERSION 0.0)

set(SOURCES )

add_executable(integration-tests ${SOURCES})

target_link_libraries(integration-tests PRIVATE sound-server-daemon)
target_link_libraries(integration-tests PRIVATE pulse-calls-mapper)
target_link_libraries(integration-tests PRIVATE GTest::gtest_main)
target_link_libraries(integration-tests PRIVATE common)