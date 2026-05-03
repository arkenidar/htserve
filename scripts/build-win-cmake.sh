#!/usr/bin/env bash
# CMake-based cross-build for Windows via MinGW-w64.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="$ROOT/build-win-cmake"

cmake -S "$ROOT" -B "$BUILD" \
    -DCMAKE_TOOLCHAIN_FILE="$ROOT/cmake/mingw-w64-x86_64.cmake" \
    -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD" --parallel

echo "built: $BUILD/htserve.exe"
file "$BUILD/htserve.exe" || true
