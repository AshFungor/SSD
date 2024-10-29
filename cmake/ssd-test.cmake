function(declare_ssd_test)
    set(options )
    set(args TEST_NAME)
    set(list_args SOURCES DEPS)

    cmake_parse_arguments(
        PARSE_ARGV 0
        ssd_test
        "${options}"
        "${args}"
        "${list_args}")

    foreach(arg IN ITEMS ${ssd_test_UNPARSED_ARGUMENTS})
        message(WARNING "SSD: Argument is not parsed: ${arg}")
    endforeach()

    declare_ssd_target(NAME ${ssd_test_TEST_NAME} TYPE EXECUTABLE SOURCES ${ssd_test_SOURCES} DEPS ${ssd_test_DEPS} GTest::gtest_main GTest::gmock)    
    add_test(NAME ${ssd_test_TEST_NAME} COMMAND ${ssd_test_TEST_NAME})

endfunction()

