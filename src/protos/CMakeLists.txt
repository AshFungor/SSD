cmake_minimum_required(VERSION 3.15)

set(PROTOS 
    # clint messages
    client/stream.proto
    client/context.proto
    # service messages
    service/stream.proto
    service/context.proto
    # common
    common/directives.proto
    common/property-list.proto
    common/stream-configuration.proto
    # holders
    holder.proto
    client-message.proto
    server-message.proto
)

# declare proto custom target and add all proto sources to it
add_library(protos OBJECT ${PROTOS})

# have to make codegen dir manually
set(PROTO_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/codegen)
file(MAKE_DIRECTORY ${PROTO_BINARY_DIR})

# specify commands for proto gen, IMPORT_DIRS - root for protos/ (imports)
protobuf_generate(
    TARGET protos                       # cmake target with sources
    LANGUAGE cpp                        # --$LANGUAGE_out option (cpp, py, grpc etc.)
    PROTOC_OUT_DIR ${PROTO_BINARY_DIR}  # output directory
    IMPORT_DIRS ${CMAKE_SOURCE_DIR}/src # imports' relative directory
)

target_link_libraries(protos PUBLIC 
    protobuf::libprotobuf
)

target_include_directories(protos PUBLIC
    $<BUILD_INTERFACE:${PROTO_BINARY_DIR}>
    # used to override system protobuf libs if necessery
    ${protobuf_INCLUDE_DIR}
)

add_library(laar::protos ALIAS protos)
