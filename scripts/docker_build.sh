#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0
set -euo pipefail
exec bash "$(dirname "$0")/docker_build_simple.sh" "$@"
