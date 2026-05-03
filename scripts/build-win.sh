#!/usr/bin/env bash
# Cross-compile htserve.exe (static x86_64) for Windows using MinGW-w64.
# Requires: sudo apt-get install -y gcc-mingw-w64-x86-64
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="$ROOT/build-win"
mkdir -p "$OUT"

if ! command -v x86_64-w64-mingw32-gcc >/dev/null; then
    echo "error: x86_64-w64-mingw32-gcc not found." >&2
    echo "install with: sudo apt-get install -y gcc-mingw-w64-x86-64" >&2
    exit 1
fi

x86_64-w64-mingw32-gcc -std=c99 -O2 -Wall -Wextra \
    -static -static-libgcc \
    "$ROOT/src/htserve.c" -o "$OUT/htserve.exe" -lws2_32

echo "built: $OUT/htserve.exe"
file "$OUT/htserve.exe" || true
