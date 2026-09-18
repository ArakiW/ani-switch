$ErrorActionPreference = 'Stop'
$root = 'E:\AI\ani-switch'
$script = Join-Path $root 'scripts\eden_stress_run.ps1'
$nro = Get-ChildItem (Join-Path $root 'release') -Recurse -Filter aniswitch.nro |
    Sort-Object LastWriteTime -Descending | Select-Object -First 1 -ExpandProperty FullName
$pairs = @(
    @{ ep = '1227087'; sid = '400602'; name = 'frieren' },
    @{ ep = '1124319'; sid = '0'; name = 'spy-s2' },
    @{ ep = '1182322'; sid = '0'; name = 'kaguya' },
    @{ ep = '730523'; sid = '0'; name = 'violet' },
    @{ ep = '176903'; sid = '0'; name = 'ggo' },
    @{ ep = '555794'; sid = '0'; name = 'oregairu' },
    @{ ep = '873959'; sid = '0'; name = 'opm-s2' }
)
$start = Get-Random -Maximum $pairs.Count
$rounds = 4
$out = Join-Path $root 'build_logs\random_play'
New-Item -ItemType Directory -Force -Path $out | Out-Null
Write-Host "NRO=$nro start=$start rounds=$rounds"
$pass = 0
for ($i = 0; $i -lt $rounds; $i++) {
    $idx = ($start + $i) % $pairs.Count
    $p = $pairs[$idx]
    $pair = "$($p.ep) $($p.sid)"
    Write-Host "==== round $($i+1)/$rounds $($p.name) pair=$pair ===="
    & powershell -NoProfile -ExecutionPolicy Bypass -File $script `
        -Mode online -OnlinePair $pair -NroPath $nro -TimeoutSec 150 -Force
    $code = $LASTEXITCODE
    $logGlob = Join-Path $root 'build_logs\stress\startup.online.log'
    $hit = $false
    if (Test-Path $logGlob) {
        $t = Get-Content $logGlob -Raw -ErrorAction SilentlyContinue
        if ($t -match 'player: file loaded|player: first frame|player: dl ok|HTTPSource: [1-9]') { $hit = $true }
        Copy-Item $logGlob (Join-Path $out "round-$($i+1)-$($p.name).log") -Force
    }
    if ($hit) { $pass++ }
    Write-Host "round $($i+1) exit=$code hit=$hit"
}
Write-Host "PASS $pass / $rounds  logs -> $out"
Get-ChildItem $out | Select-Object Name, Length
