cmake_minimum_required(VERSION 3.15)

project(protos VERSION 0.0)

add_subdirectory(channel)
add_subdirectory(client)

add_library(protos OBJECT ${PROTOS})
protobuf_generate(TARGET protos IMPORT_DIRS ${PROTOS_SOURCE_ROOT})

foreach(proto-hdr IN ITEMS ${HEADERS})
    message(STATUS "Protobuf: header generated ${proto-hdr}")
endforeach()

target_link_libraries(protos PUBLIC protobuf::libprotobuf)

set(PROTO_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_include_directories(protos PUBLIC 
    ${PROTOS_BINARY_ROOT}
    "$<BUILD_INTERFACE:${PROTO_BINARY_DIR}>" 
    ${protobuf_INCLUDE_DIR})