#!/bin/bash
set -e

BUILD_TYPE="${1:-Release}"
BUILD_DIR="build"

echo "Building oatmux ($BUILD_TYPE)..."

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

cmake .. -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
make -j"$(nproc)"

echo ""
echo "Build complete: $BUILD_DIR/oatmux"
echo "Run with: ./build/oatmux -s <session-name>"
