#!/bin/bash
set -euo pipefail
cd /proj
cmake --build cmake-build-switch --target aniswitch.nro -j 8
TS="$(TZ=Asia/Shanghai date +%Y%m%d-%H%M%S)"
REL="/proj/release/${TS}"
mkdir -p "$REL"
cp cmake-build-switch/aniswitch.nro "$REL/"
cp cmake-build-switch/platform/switch/aniswitch.nacp "$REL/" 2>/dev/null || true
cp cmake-build-switch/aniswitch.nro /proj/aniswitch.nro
sha256sum "$REL/aniswitch.nro"
echo "RELEASE_DIR=$REL"
ls -la "$REL"
