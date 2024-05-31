#!/bin/bash

set -e

# Must be root of project (relative to highest CMakeLists.txt)
ROOT_FOLDER="$PWD"

# Output dir, adjusted to target platform
BUILD_DIR="$ROOT_FOLDER/build"

echo "testing build type variable..."
if [[ -z $BUILD_TYPE ]]; then
    BUILD_TYPE=Debug
fi

BUILD_TYPE_LOWERCASE=$(echo "$BUILD_TYPE" | tr '[:upper:]' '[:lower:]')


help() {
    echo "helper for building Sound Server. Usage: [-c|configure] [-b|build]"
}

configure() {
    echo "Writing to dir: $BUILD_DIR, target platform is $ARCH"
    echo "Configuring..."
    test -d "$BUILD_DIR" || mkdir -p "$BUILD_DIR"
    echo "Checking Conan installation..." && test -n "$(whereis conan)"

    conan install "$ROOT_FOLDER" --output-folder="$BUILD_DIR" --build=missing $CONAN_ARGS

    cd "$BUILD_DIR"                                     \
        && cmake --preset conan-$BUILD_TYPE_LOWERCASE   \
            "$ROOT_FOLDER" -G "Unix Makefiles"          \
            -DCMAKE_BUILD_TYPE=$BUILD_TYPE
            
}

build() {
    test -d "$BUILD_DIR"
    echo "Building..."
    cmake --build "$BUILD_DIR" --preset conan-$BUILD_TYPE_LOWERCASE
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
