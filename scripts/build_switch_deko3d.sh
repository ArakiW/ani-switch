#!/usr/bin/env bash
# ani-switch Switch build (deko3d / hardware decode)
# Adapted from xfangfang/wiliwili (GPL-3.0) scripts/build_switch_deko3d.sh
#
# Requires a deko3d-enabled libmpv. The wiliwili maintainer publishes
# prebuilt packages on their release page.
#
# Usage:
#   DEVKITPRO=/path/to/devkitpro ./scripts/build_switch_deko3d.sh

set -euo pipefail

BUILD_DIR=cmake-build-switch-deko3d

cd "$(dirname "$0")/.."

BASE_URL="https://github.com/xfangfang/wiliwili/releases/download/v0.1.0/"

PKGS=(
    "libuam-f8c9eef01ffe06334d530393d636d69e2b52744b-1-any.pkg.tar.zst"
    "switch-ffmpeg-7.1-1-any.pkg.tar.zst"
    "switch-libmpv_deko3d-0.36.0-2-any.pkg.tar.zst"
    "switch-nspmini-48d4fc2-1-any.pkg.tar.xz"
    "hacBrewPack-3.05-1-any.pkg.tar.zst"
)
for PKG in "${PKGS[@]}"; do
    [ -f "${PKG}" ] || curl -LO ${BASE_URL}${PKG}
    dkp-pacman -U --noconfirm "${PKG}"
done

cmake -B "${BUILD_DIR}" \
      -DCMAKE_BUILD_TYPE=Release \
      -DPLATFORM_SWITCH=ON \
      -DUSE_DEKO3D=ON \
      -DUSE_GLFW=OFF \
      -DANISWITCH_BUILD_TESTS=OFF \
      -DCMAKE_UNITY_BUILD=OFF

make -C "${BUILD_DIR}" aniswitch.nro -j"$(nproc)"

echo "Done: ${BUILD_DIR}/aniswitch.nro"
