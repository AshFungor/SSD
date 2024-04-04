#!/bin/sh

set -e

# Must be root of project (relative to highest CMakeLists.txt)
ROOT_FOLDER="$PWD"

# Output dir, adjusted to target platform
BUILD_DIR="$ROOT_FOLDER/build"

# Available arch targets
ARCH_TARGETS=("arm64" "system" "x86_64")

# default target (auto system profile)
ARCH="system"

help() {
    echo "helper for building Sound Server. Usage: [-c|configure] [-b|build] [-a:ARCH|architecture]"
    echo "architecture may be either \"system\", \"arm64\" or \"x86_64\""
}

configure() {
    echo "Writing to dir: $BUILD_DIR, target platform is $ARCH"
    echo "Configuring..."
    test -d "$BUILD_DIR" || mkdir -p "$BUILD_DIR"
    echo "Checking Conan installation..." && test -n "$(whereis conan)"
    # match arch for deps
    if [ "$ARCH" = "system" ]; then
        PROFILE=""
        COMPILER=""
    elif [ "$ARCH" = "x86_64" ]; then
        PROFILE=""
        COMPILER=""
        # PROFILE="$ROOT_FOLDER/data/x86_64-profile.txt"
        # COMPILER_C="gcc"
        # COMPILER_CXX="g++"
        # SYSTEM="x86_64"
    elif [ "$ARCH" = "arm64" ]; then
        PROFILE="$ROOT_FOLDER/data/arm64-profile.txt"
        COMPILER_C="aarch64-linux-gnu-gcc"
        COMPILER_CXX="aarch64-linux-gnu-g++"
        SYSTEM="arm"
    fi

    test -n "$PROFILE" \
        && conan install "$ROOT_FOLDER" --output-folder="$BUILD_DIR" --build=missing -pr $PROFILE \
        || conan install "$ROOT_FOLDER" --output-folder="$BUILD_DIR" --build=missing

    cd "$BUILD_DIR" && test -n "$SYSTEM"                                \
        && cmake                                                        \
            "$ROOT_FOLDER" -G                                           \
            "Unix Makefiles" -DCMAKE_BUILD_TYPE=Debug                   \
            -DCMAKE_TOOLCHAIN_FILE="$BUILD_DIR/conan_toolchain.cmake"   \
            -DCMAKE_SYSTEM_NAME=Generic                                 \
            -DCMAKE_SYSTEM_PROCESSOR="$SYSTEM"                          \
            -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY              \
        || cmake                                                        \
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
    if [ "$arg" = "a" ]; then
        test -n "$(echo $ARCH_TARGETS | grep "$OPTARG")" && ARCH="$OPTARG"
    elif [ "$arg" = "b" ]; then
        BUILD_DIR="$BUILD_DIR"_"$ARCH"
        build
    elif [ "$arg" = "h" ]; then
        help
    elif [ "$arg" = "c" ]; then
        test "$ARCH" = "system" && BUILD_DIR="$BUILD_DIR" || BUILD_DIR="$BUILD_DIR"_"$ARCH"
        configure
    else
        return 1
    fi
done
