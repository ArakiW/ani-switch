$ErrorActionPreference = 'Stop'
$root = 'E:\AI\ani-switch'
$nro = Join-Path $root 'release\20260918-191525\aniswitch.nro'
$script = Join-Path $root 'scripts\eden_stress_run.ps1'
$out = Join-Path $root 'build_logs\playtest'
New-Item -ItemType Directory -Force -Path $out | Out-Null
Write-Host "NRO=$nro"
& powershell -NoProfile -ExecutionPolicy Bypass -File $script -Mode playtestd -OnlinePair '1227087 400602' -NroPath $nro -TimeoutSec 90 -Force
$log = Join-Path $root 'build_logs\stress\startup.playtestd.log'
if (Test-Path $log) {
    Copy-Item $log (Join-Path $out 'exp-direct2.log') -Force
    Select-String -Path $log -Pattern 'AUTOTOUR|mpv-direct|allowNet|NETWORK|PLAYTEST|SHLS|playlist duration|stream mode|setUrl|file loaded|picker' |
        ForEach-Object { $_.Line }
}
