#!/bin/bash
set -e

BUILD_DIR="build"

# Handle install command
if [ "$1" = "install" ]; then
    if [ ! -f "$BUILD_DIR/oatmux" ]; then
        echo "Build first: ./compile.sh"
        exit 1
    fi
    cd "$BUILD_DIR"
    sudo make install
    echo "Installed to /usr/local/bin/oatmux"
    exit 0
fi

# Handle clean command
if [ "$1" = "clean" ]; then
    rm -rf "$BUILD_DIR"
    echo "Cleaned."
    exit 0
fi

BUILD_TYPE="${1:-Release}"

echo "Building oatmux ($BUILD_TYPE)..."

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

cmake .. -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
make -j"$(nproc)"

echo ""
echo "Build complete: $BUILD_DIR/oatmux"
echo ""
echo "Run:     ./build/oatmux"
echo "Install: ./compile.sh install"
