cmake_minimum_required(VERSION 3.15)

project(contrib LANGUAGES CXX C VERSION 0.0)

# RtAudio
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build rtaudio shared library" FORCE)

# grpc, generic options
set(gRPC_BUILD_TESTS OFF CACHE STRING "Build tests for grpc" FORCE)
# grpc, package providers for tuning deps
set(gRPC_ZLIB_PROVIDER "package" CACHE STRING "Provider of zlib library" FORCE)
set(gRPC_CARES_PROVIDER "package" CACHE STRING "Provider of c-ares library" FORCE)
set(gRPC_RE2_PROVIDER "package" CACHE STRING "Provider of re2 library" FORCE)
set(gRPC_SSL_PROVIDER "package" CACHE STRING "Provider of ssl library" FORCE)
set(gRPC_PROTOBUF_PROVIDER "package" CACHE STRING "Provider of protobuf library" FORCE)
set(gRPC_ABSL_PROVIDER "package" CACHE STRING "Provider of absl library" FORCE)

add_subdirectory(grpc)
add_subdirectory(rtaudio)
add_subdirectory(AudioFile)

# aliases to account to namespaces
add_library(gRPC::grpc++ ALIAS grpc++)
add_library(gRPC::grpc ALIAS grpc)