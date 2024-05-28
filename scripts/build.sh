#!/bin/bash

set -e

# Must be root of project (relative to highest CMakeLists.txt)
ROOT_FOLDER="$PWD"

# Output dir, adjusted to target platform
BUILD_DIR="$ROOT_FOLDER/build"

help() {
    echo "helper for building Sound Server. Usage: [-c|configure] [-b|build]"
}

configure() {
    echo "Writing to dir: $BUILD_DIR, target platform is $ARCH"
    echo "Configuring..."
    test -d "$BUILD_DIR" || mkdir -p "$BUILD_DIR"
    echo "Checking Conan installation..." && test -n "$(whereis conan)"

    conan install "$ROOT_FOLDER" --output-folder="$BUILD_DIR" --build=missing $CONAN_ARGS

    cd "$BUILD_DIR"                                                     \
        && cmake                                                        \
            "$ROOT_FOLDER" -G                                           \
            "Unix Makefiles" -DCMAKE_BUILD_TYPE=Debug                   \
            -DCMAKE_TOOLCHAIN_FILE="$BUILD_DIR/conan_toolchain.cmake"
}

build() {
    test -d "$BUILD_DIR" || configure
    echo "Building..."
    cmake --build "$BUILD_DIR"
}

while getopts "hcba:" arg; do 
    if [ "$arg" = "b" ]; then
        build
    elif [ "$arg" = "h" ]; then
        help
    elif [ "$arg" = "c" ]; then
        configure
    else
        return 1
    fi
done
