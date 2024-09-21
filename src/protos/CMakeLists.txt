cmake_minimum_required(VERSION 3.15)

set(PROTOS 
    # clint messages
    client/base.proto
    # service messages
    service/base.proto
    # holders
    client-message.proto
    server-message.proto
    # services
    services/sound-router.proto
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

protobuf_generate(
    TARGET protos                                           # ...
    LANGUAGE grpc                                           # ...
    GENERATE_EXTENSIONS .grpc.pb.h .grpc.pb.cc              # file extensions from generator
    IMPORT_DIRS ${CMAKE_SOURCE_DIR}/src                     # ...
    PROTOC_OUT_DIR ${PROTO_BINARY_DIR}                      # ...
    PLUGIN "protoc-gen-grpc=$<TARGET_FILE:grpc_cpp_plugin>" # gRPC plugin executable
)

# adjust deps to correctly assemble grpc_cpp_plugin before codegen
add_dependencies(protos grpc_cpp_plugin)

target_link_libraries(protos PUBLIC 
    protobuf::libprotobuf
    gRPC::grpc++
    gRPC::grpc
)

target_include_directories(protos PUBLIC
    $<BUILD_INTERFACE:${PROTO_BINARY_DIR}>
    # used to override system protobuf libs if necessery
    ${protobuf_INCLUDE_DIR}
)
