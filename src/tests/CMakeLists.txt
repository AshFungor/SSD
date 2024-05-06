cmake_minimum_required(VERSION 3.15)

project(tests LANGUAGES CXX VERSION 0.0)

set(SOURCES protocol-test.cpp)
set(HEADERS mocked-sound-handler.hpp)

add_executable(integration-tests ${SOURCES} ${HEADERS})

target_link_libraries(integration-tests PRIVATE server)
target_link_libraries(integration-tests PRIVATE sound)
target_link_libraries(integration-tests PRIVATE GTest::gtest_main)
target_link_libraries(integration-tests PRIVATE GTest::gmock_main)
target_link_libraries(integration-tests PRIVATE common)