# SPDX-License-Identifier: AGPL-3.0
# Unit test for Get-StressSummary parser extracted from eden_stress_run.ps1
$ErrorActionPreference = 'Stop'
$src = Get-Content "E:\AI\ani-switch\scripts\eden_stress_run.ps1" -Raw
$start = $src.IndexOf('function Get-StressSummary')
$end = $src.IndexOf('function Write-SummaryArtifacts')
if ($start -lt 0 -or $end -lt 0) { throw "function bounds not found start=$start end=$end" }
$fn = $src.Substring($start, $end - $start)
$tmp = Join-Path $env:TEMP 'stress_parse_test.ps1'
Set-Content -Path $tmp -Value $fn -Encoding UTF8
. $tmp

$sampleLines = @(
  'STRESS: START cycles=3 battery=14 (no player/onboarding)',
  'STRESS: BEGIN cycle 1/3 depth=1',
  'STRESS: open settings c=1 s=0 depth=1',
  'STRESS: opened settings depth=2',
  'STRESS: cycle 1/3 complete depth=1 fails=0',
  'STRESS: BEGIN cycle 2/3 depth=1',
  'STRESS: FAIL settings settle depth=2 expected=1',
  'STRESS: cycle 2/3 complete depth=1 fails=1',
  'STRESS: BEGIN cycle 3/3 depth=1',
  'STRESS: cycle 3/3 complete depth=1 fails=1',
  'STRESS: DONE cycles=3 fails=0 peakDepth=3'
)
# Note: DONE fails=0 is what the app logs when finishStress sees fails==0.
# If intermediate FAILs incremented g_fails, app logs FAIL cycles= instead.
# For parser: terminal DONE fails=0 wins.

$sampleClean = @(
  'STRESS: START cycles=2 battery=14 (no player/onboarding)',
  'STRESS: BEGIN cycle 1/2 depth=1',
  'STRESS: open settings c=1 s=0 depth=1',
  'STRESS: opened settings depth=2',
  'STRESS: cycle 1/2 complete depth=1 fails=0',
  'STRESS: BEGIN cycle 2/2 depth=1',
  'STRESS: cycle 2/2 complete depth=1 fails=0',
  'STRESS: DONE cycles=2 fails=0 peakDepth=3'
)
$sampleFail = @(
  'STRESS: START cycles=2 battery=14 (no player/onboarding)',
  'STRESS: BEGIN cycle 1/2 depth=1',
  'STRESS: FAIL collection open exception boom',
  'STRESS: cycle 1/2 complete depth=1 fails=1',
  'STRESS: FAIL cycles=2 fails=1 peakDepth=3 endDepth=2'
)

$s0 = Get-StressSummary -Lines $sampleClean
Write-Host ("CLEAN done={0} termFail={1} fails={2} peak={3} cyclesSeen={4} cyclesReq={5} open={6}" -f `
  $s0.done, $s0.terminal_fail, $s0.fail_count, $s0.peak_depth, $s0.cycles_seen, $s0.cycles_requested, $s0.open_count)
if (-not $s0.done) { throw "CLEAN expected DONE" }
if ($s0.fail_count -ne 0) { throw "CLEAN fail_count!=0" }
if ($s0.peak_depth -ne 3) { throw "CLEAN peakDepth!=3 got $($s0.peak_depth)" }
if ($s0.cycles_seen -ne 2) { throw "CLEAN cycles_seen!=2 got $($s0.cycles_seen)" }
if ($s0.cycles_requested -ne 2) { throw "CLEAN cycles_requested!=2" }

$s1 = Get-StressSummary -Lines $sampleLines
Write-Host ("DONE-with-intermediate done={0} fails={1} peak={2} cyclesSeen={3} failMarkers={4}" -f `
  $s1.done, $s1.fail_count, $s1.peak_depth, $s1.cycles_seen, $s1.fail_markers)
if (-not $s1.done) { throw "S1 expected DONE" }
# terminal DONE fails=0 is authoritative even if intermediate FAIL lines exist
if ($s1.fail_count -ne 0) { throw "S1 expected fail_count=0 (DONE fails=0) got $($s1.fail_count)" }
if ($s1.cycles_seen -ne 3) { throw "S1 cycles_seen!=3 got $($s1.cycles_seen)" }
if ($s1.peak_depth -ne 3) { throw "S1 peakDepth!=3" }

$s2 = Get-StressSummary -Lines $sampleFail
Write-Host ("FAIL done={0} termFail={1} fails={2} peak={3} end={4} cyclesSeen={5}" -f `
  $s2.done, $s2.terminal_fail, $s2.fail_count, $s2.peak_depth, $s2.end_depth, $s2.cycles_seen)
if (-not $s2.terminal_fail) { throw "S2 expected terminal_fail" }
if ($s2.done) { throw "S2 should not be DONE" }
if ($s2.fail_count -ne 1) { throw "S2 fail_count!=1 got $($s2.fail_count)" }
if ($s2.peak_depth -ne 3) { throw "S2 peakDepth!=3 got $($s2.peak_depth)" }
if ($s2.end_depth -ne 2) { throw "S2 endDepth!=2 got $($s2.end_depth)" }
if ($s2.cycles_seen -ne 2) { throw "S2 cycles_seen!=2 got $($s2.cycles_seen)" }

Write-Host "PARSER TESTS OK"
