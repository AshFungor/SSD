cmake_minimum_required(VERSION 3.5)

project(protos VERSION 0.0)

set(PROTOS hello-world.proto)
protobuf_generate_cpp(SOURCES HEADERS ${PROTOS})

foreach(proto-hdr IN ITEMS ${HEADERS})
    message(STATUS "Protobuf: header generated ${proto-hdr}")
endforeach()

message(STATUS ${PROTOBUF_PROTOC_EXECUTABLE})

add_library(protos STATIC ${SOURCES} ${HEADERS})

target_include_directories(protos PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(protos PRIVATE protobuf::protobuf)