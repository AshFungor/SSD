function(declare_ssd_target)
    set(options )
    set(args NAME TYPE)
    set(list_args SOURCES DEPS)

    cmake_parse_arguments(
        PARSE_ARGV 0
        ssd_target
        "${options}"
        "${args}"
        "${list_args}")

    foreach(arg IN ITEMS ${ssd_target_UNPARSED_ARGUMENTS})
        message(WARNING "SSD: Argument is not parsed: ${arg}")
    endforeach()

    message(STATUS "SSD: declaring target of name: \'${ssd_target_NAME}\' of type: \'${ssd_target_TYPE}\'")

    set(DEPS ${SSD_EXTERNAL_DEPS} ${ssd_target_DEPS})

    if("${ssd_target_TYPE}" STREQUAL "STATIC")
        message(STATUS "SSD: declaring static library of name: \'${ssd_target_NAME}\'")
        add_library(${ssd_target_NAME} STATIC)
    elseif("${ssd_target_TYPE}" STREQUAL "SHARED")
        message(STATUS "SSD: declaring shared library of name: \'${ssd_target_NAME}\'")
        add_library(${ssd_target_NAME} SHARED)
    elseif("${ssd_target_TYPE}" STREQUAL "EXECUTABLE")
        message(STATUS "SSD: declaring executable of name: \'${ssd_target_NAME}\'")
        add_executable(${ssd_target_NAME})
    elseif("${ssd_target_TYPE}" STREQUAL "INTERFACE")
        message(STATUS "SSD: declaring interface library of name: \'${ssd_target_NAME}\'")
        add_library(${ssd_target_NAME} INTERFACE)
    else()
        message(FATAL_ERROR "failed to deduce type of target: \'${ssd_target_NAME}\'")
    endif()

    target_link_libraries(${ssd_target_NAME} PUBLIC ${DEPS})
    target_include_directories(${ssd_target_NAME} PUBLIC ${CMAKE_SOURCE_DIR})

    target_sources(${ssd_target_NAME} PRIVATE ${ssd_target_SOURCES})
    # Add -Wall and -Wextra if possible
    if (CMAKE_COMPILER_IS_GNUCXX)
        target_compile_options(${ssd_target_NAME} PUBLIC -Wall -Wextra)
    endif()

endfunction()