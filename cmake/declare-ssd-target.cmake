# LINK_WITH_GTEST_MAIN - weather target is test and requires gtest with main
# LINK_WITH_GTEST - weather target is test and requires gtest
# NAME - target name
# TYPE - target type: executable, library

function(declare_ssd_target)
    set(options LINK_WITH_GTEST_MAIN LINK_WITH_GTEST)
    set(args NAME TYPE)
    set(list_args SOURCES HEADERS)

    cmake_parse_arguments(
        PARSE_ARGV 0
        ssd_target
        "${options}"
        "${args}"
        "${list_args}")

    foreach(arg IN ITEMS ${ssd_target_UNPARSED_ARGUMENTS})
        message(WARNING "[declare_ssd_target] Argument is not parsed: ${arg}")
    endforeach()

endfunction()