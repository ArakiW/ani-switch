$ErrorActionPreference = 'Stop'
$root = 'E:\AI\ani-switch'
$script = Join-Path $root 'scripts\eden_stress_run.ps1'
$nro = Join-Path $root 'release\20260918-181102\aniswitch.nro'
# Working sources.json episode keys
$pairs = @(
    @{ ep = '1227087'; sid = '400602'; name = 'frieren' },
    @{ ep = '1124319'; sid = '0'; name = 'spy-s2' },
    @{ ep = '1182322'; sid = '0'; name = 'kaguya' }
)
$out = Join-Path $root 'build_logs\playtest'
New-Item -ItemType Directory -Force -Path $out | Out-Null
Write-Host "NRO=$nro"
$pass = 0
$idx = 0
foreach ($p in $pairs) {
    $idx++
    $pair = "$($p.ep) $($p.sid)"
    Write-Host "==== playtest $idx/$($pairs.Count) $($p.name) $pair ===="
    & powershell -NoProfile -ExecutionPolicy Bypass -File $script `
        -Mode playtest -OnlinePair $pair -NroPath $nro -TimeoutSec 200 -Force
    $log = Join-Path $root 'build_logs\stress\startup.playtest.log'
    if (Test-Path $log) {
        Copy-Item $log (Join-Path $out "playtest-$($p.name).log") -Force
        $t = Get-Content $log -Raw
        $done = $t -match 'PLAYTEST:\s*DONE'
        $seeks = ([regex]::Matches($t, 'PLAYTEST: seek ')).Count
        $shls = $t -match 'SHLS:'
        $fileLoaded = $t -match 'player: file loaded|SHLS: open ok'
        Write-Host "  result done=$done seeks=$seeks seamless=$shls loaded=$fileLoaded"
        if ($done -and $seeks -ge 4) { $pass++ }
        Select-String -Path $log -Pattern 'PLAYTEST:|SHLS:|player: file|player: setUrl' |
            Select-Object -Last 25 | ForEach-Object { "  $($_.Line)" }
    } else {
        Write-Host "  no playtest log"
    }
}
Write-Host "PLAYTEST PASS $pass / $($pairs.Count)  logs -> $out"
Get-ChildItem $out | Select-Object Name, Length
