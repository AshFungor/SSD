cmake_minimum_required(VERSION 3.15)

project(protos VERSION 0.0)

set(PROTOS hello-world.proto)

protobuf_generate_cpp(SOURCES HEADERS ${PROTOS})

foreach(proto-hdr IN ITEMS ${HEADERS})
    message(STATUS "Protobuf: header generated ${proto-hdr}")
endforeach()

add_library(protos STATIC ${SOURCES} ${HEADERS})

target_link_libraries(protos PRIVATE protobuf::libprotobuf)

target_include_directories(protos 
    PUBLIC ${CMAKE_CURRENT_BINARY_DIR} ${protobuf_INCLUDE_DIR})