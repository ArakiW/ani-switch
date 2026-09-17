#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="$ROOT/cmake-build-switch"
SWITCH_DEBUG=${ANISWITCH_SWITCH_DEBUG:-ON}

mkdir -p "$ROOT/build_logs"
cmake -S "$ROOT" -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DPLATFORM_SWITCH=ON \
    -DUSE_GLFW=ON \
    -DUSE_DEKO3D=OFF \
    -DANISWITCH_BUILD_TESTS=OFF \
    -DBRLS_UNITY_BUILD=ON \
    -DCMAKE_UNITY_BUILD=OFF \
    -DANISWITCH_SWITCH_DEBUG="$SWITCH_DEBUG" \
    2>&1 | tee "$ROOT/build_logs/configure.log"

cmake --build "$BUILD_DIR" --target aniswitch.nro --parallel "${BUILD_JOBS:-$(nproc)}" \
    2>&1 | tee "$ROOT/build_logs/make.log"

test -s "$BUILD_DIR/aniswitch.nro"
cp "$BUILD_DIR/aniswitch.nro" "$ROOT/aniswitch.nro"
sha256sum "$ROOT/aniswitch.nro"

# Stash NRO + NACP into release/<UTC-timestamp>/ for easy SD-card pickup.
# Use the container's local time (driven by the TZ env var that
# build_switch.sh passes through, defaulting to Asia/Shanghai).  The
# earlier `date -u` produced UTC-stamped directories whose names read
# 8 hours behind the user's wall clock when sorted alphabetically.
TS="${RELEASE_TS:-$(TZ=${TZ:-Asia/Shanghai} date +%Y%m%d-%H%M%S)}"
REL="$ROOT/release/$TS"
mkdir -p "$REL"
cp "$BUILD_DIR/aniswitch.nro"          "$REL/aniswitch.nro"
cp "$BUILD_DIR/platform/switch/aniswitch.nacp" "$REL/aniswitch.nacp"
# v16.10.11: ship the CA bundle next to the NRO so the user can
# drop it at sdmc:/switch/aniswitch/ca-bundle.crt without having
# to download it from curl.se themselves.  cpr::SslOptions::ca_path
# points there so libcurl can verify the Switch-mbedTLS handshake
# without relying on a system cert store.
if [ -f "$ROOT/resources/ca-bundle.crt" ]; then
    cp "$ROOT/resources/ca-bundle.crt" "$REL/ca-bundle.crt"
fi
# v20.14: ship a short local test video so the user can exercise
# PlayerActivity without downloading anything.  Copy to
# sdmc:/switch/aniswitch/videos/ on the SD card.
mkdir -p "$REL/videos"
if [ -f "$ROOT/dist/sample-5s.mp4" ]; then
    cp "$ROOT/dist/sample-5s.mp4" "$REL/videos/test-local.mp4"
fi
( cd "$REL" && sha256sum aniswitch.nro aniswitch.nacp ca-bundle.crt > SHA256SUMS )
if [ -f "$REL/videos/test-local.mp4" ]; then
    ( cd "$REL" && sha256sum videos/test-local.mp4 >> SHA256SUMS )
fi
if git -C "$ROOT" rev-parse --short HEAD >/dev/null 2>&1; then
    git -C "$ROOT" rev-parse --short HEAD            > "$REL/git-rev"
    git -C "$ROOT" log -1 --pretty='%h %s%n%ci'      > "$REL/git-log"
else
    printf '[release] note: no git HEAD in %s, writing empty git-rev/git-log\n' "$ROOT"
    : > "$REL/git-rev"
    : > "$REL/git-log"
fi
echo "[release] $REL"
ls -la "$REL"
