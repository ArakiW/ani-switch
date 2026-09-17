#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0
set -euo pipefail
# Pin the container's local time to the host timezone so release
# directory names match the user's wall clock.  Override with
# `TZ=UTC build_switch.sh` if you need UTC-stamped archives.
export TZ="${TZ:-Asia/Shanghai}"
exec bash "$(dirname "$0")/docker_build_simple.sh" "$@"
