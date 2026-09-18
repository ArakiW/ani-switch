$ErrorActionPreference = 'Stop'
$root = 'E:\AI\ani-switch'
$nro = Join-Path $root 'release\20260918-190726\aniswitch.nro'
$script = Join-Path $root 'scripts\eden_stress_run.ps1'
$out = Join-Path $root 'build_logs\playtest'
New-Item -ItemType Directory -Force -Path $out | Out-Null
$pair = '1227087 400602'

Write-Host '==== EXP A: mpv-direct (wiliwili network loadfile) ===='
& powershell -NoProfile -ExecutionPolicy Bypass -File $script -Mode playtestd -OnlinePair $pair -NroPath $nro -TimeoutSec 120 -Force
$logA = Join-Path $root 'build_logs\stress\startup.playtestd.log'
if (Test-Path $logA) {
    Copy-Item $logA (Join-Path $out 'exp-direct.log') -Force
    Select-String -Path $logA -Pattern 'mpv-direct|allowNet|NETWORK|SHLS:|PLAYTEST:|file loaded|playlist duration|stream mode|REJECTED' |
        Select-Object -Last 40 | ForEach-Object { $_.Line }
}

Write-Host '==== EXP B: seamless control ===='
& powershell -NoProfile -ExecutionPolicy Bypass -File $script -Mode playtest -OnlinePair $pair -NroPath $nro -TimeoutSec 120 -Force
$logB = Join-Path $root 'build_logs\stress\startup.playtest.log'
if (Test-Path $logB) {
    Copy-Item $logB (Join-Path $out 'exp-seamless.log') -Force
    Select-String -Path $logB -Pattern 'mpv-direct|SHLS:|PLAYTEST:|file loaded|playlist duration|stream mode|seamless' |
        Select-Object -Last 30 | ForEach-Object { $_.Line }
}
Write-Host 'EXPERIMENT DONE logs ->' $out
