# LINK_WITH_GTEST_MAIN - weather target is test and requires gtest with main
# LINK_WITH_GTEST - weather target is test and requires gtest
# NAME - target name
# TYPE - target type: executable, library

function(declare_ssd_target)
    set(options LINK_WITH_GTEST_MAIN LINK_WITH_GTEST TEST)
    set(args NAME TYPE)
    set(list_args SOURCES HEADERS LOCAL_DEPS)

    cmake_parse_arguments(
        PARSE_ARGV 0
        ssd_target
        "${options}"
        "${args}"
        "${list_args}")

    if(ssd_target_TEST)
        set(ssd_target_LINK_WITH_GTEST_MAIN ON)
        set(ssd_target_LINK_WITH_GTEST OFF)
    endif()

    foreach(arg IN ITEMS ${ssd_target_UNPARSED_ARGUMENTS})
        message(WARNING "[declare_ssd_target] Argument is not parsed: ${arg}")
    endforeach()

    set(DEPS ${SSD_EXTERNAL_DEPS} ${ssd_target_LOCAL_DEPS})
    if(ssd_target_LINK_WITH_GTEST_MAIN)
        list(APPEND DEPS GTest::gtest_main)
    endif()
    if(ssd_target_LINK_WITH_GTEST)
        list(APPEND DEPS Gtest::gtest)
    endif()

    if("${ssd_target_TYPE}" EQUAL "STATIC")
        add_library(${ssd_target_NAME} STATIC)
    elseif("${ssd_target_TYPE}" EQUAL "SHARED")
        add_library(${ssd_target_NAME} SHARED)
    elseif("${ssd_target_TYPE}" EQUAL "EXECUTABLE")
        add_executable(${ssd_target_NAME})
    elseif("${ssd_target_TYPE}" EQUAL "INTERFACE")
        add_library(${ssd_target_NAME} INTERFACE)
    endif()

    target_sources(${ssd_target_NAME} ${ssd_target_SOURCES} ${ssd_target_HEADERS})
    # Add -Wall and -Wextra if possible
    if (CMAKE_COMPILER_IS_GNUCXX)
        target_compile_options(${ssd_target_NAME} "-Wall -Wextra")
    endif()

endfunction()