#!/usr/bin/env bash
# ani-switch desktop build (Linux / macOS / Windows MSYS2)
# Used for local development without devkitpro.
#
# Linux:
#   sudo apt install libmpv-dev libwebp-dev libssl-dev cmake
#   ./scripts/build_desktop.sh
#
# macOS:
#   brew install mpv webp
#   ./scripts/build_desktop.sh
#
# Windows (MSYS2 MinGW64):
#   pacman -S mingw-w64-x86_64-{gcc,cmake,make,mpv,libwebp}
#   ./scripts/build_desktop.sh

set -e

BUILD_DIR=cmake-build-desktop

cd "$(dirname "$0")/.."

cmake -B "${BUILD_DIR}" \
      -DCMAKE_BUILD_TYPE=Debug \
      -DPLATFORM_DESKTOP=ON \
      -DUSE_GLFW=ON \
      -DANISWITCH_BUILD_TESTS=ON

cmake --build "${BUILD_DIR}" -j"$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)"

cd "${BUILD_DIR}"
ctest --output-on-failure
