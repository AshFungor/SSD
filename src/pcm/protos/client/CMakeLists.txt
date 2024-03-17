cmake_minimum_required(VERSION 3.15)

project(protos VERSION 0.0)

set(PROTOS "${CMAKE_CURRENT_SOURCE_DIR}/client-message.proto")
set(PROTOS ${PROTOS} "${CMAKE_CURRENT_SOURCE_DIR}/simple/simple.proto")

set(PROTOS ${PROTOS} PARENT_SCOPE)