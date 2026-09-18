#!/bin/bash
# Multi-round random anime online play smoke on Eden.
# Autotour mode "online" + line2 "<episodeId> <subjectId>"
set -euo pipefail
ROOT="/proj"
# Keys that previously PLAY_OK on device/emulator (episodeId subjectId)
PAIRS=(
  "1227087 400602"
  "1124319 0"
  "1182322 0"
  "730523 0"
  "176903 0"
  "555794 0"
  "873959 0"
)
# Shuffle-ish: rotate start index
START=$((RANDOM % ${#PAIRS[@]}))
COUNT=${1:-4}
OUT="$ROOT/build_logs/random_play"
mkdir -p "$OUT"
NRO=$(ls -1 "$ROOT"/release/*/aniswitch.nro 2>/dev/null | sort | tail -1)
echo "NRO=$NRO rounds=$COUNT start=$START"
for ((i=0; i<COUNT; i++)); do
  idx=$(( (START + i) % ${#PAIRS[@]} ))
  pair="${PAIRS[$idx]}"
  echo "==== round $((i+1))/$COUNT  $pair ===="
  powershell -NoProfile -ExecutionPolicy Bypass -File \
    "$ROOT/scripts/eden_stress_run.ps1" \
    -Mode online -NroPath "$NRO" -TimeoutSec 180 -Force \
    -ExtraArgs "$pair" 2>&1 | tee "$OUT/round-$((i+1)).log" || true
  # Fallback: write autotour manually if ExtraArgs unsupported
  EDEN=$(ls -d /e/AI/*Eden*/Eden-v*/Eden-Windows-v* 2>/dev/null | head -1 || true)
done
echo "done rounds → $OUT"
