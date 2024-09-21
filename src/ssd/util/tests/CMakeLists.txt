cmake_minimum_required(VERSION 3.15)

set(SOURCES config-loader-test.cpp)

add_executable(utils-test ${SOURCES})

target_compile_definitions(utils-test PRIVATE BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}/")

target_link_libraries(utils-test PRIVATE 
    utils
    common
    GTest::gtest_main
    nlohmann_json::nlohmann_json
)
