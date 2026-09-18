# SPDX-License-Identifier: AGPL-3.0
<#
.SYNOPSIS
    Host-side Eden runner for ani-switch autotour / navigation stress.

.DESCRIPTION
    Deploys aniswitch.nro into Eden's virtual sdmc, writes the autotour
    control file, launches Eden, polls startup.log for STRESS markers,
    captures window screenshots, and summarizes fails / peakDepth.

    Input injection into the Eden game window does NOT work — do not
    SendKeys. Autotour is the only automation path.

    Eden launch method (repo search + on-disk):
      - No prior documented launch command in build_logs/docs/notes.
      - Install has eden.exe (Qt GUI) + eden-cli.exe.
      - Try order:
          1) eden-cli.exe "<host-path-to-nro>"
          2) eden.exe     "<host-path-to-nro>"
          3) eden.exe     (GUI only — user must launch aniswitch title)
      - NRO copied to BOTH known sdmc locations:
          user/sdmc/switch/aniswitch/aniswitch.nro   (AGENTS.md path)
          user/sdmc/switch/aniswitch.nro             (also present on disk)

    Autotour file (ASCII, no BOM) at:
      <eden>/user/sdmc/switch/aniswitch/autotour
      line1 = mode  (stress50 | smoke | settings | collection | ...)
      line2 = optional count (stress50 writes Cycles, e.g. 50)

    REQUIRES: NRO built with -DANISWITCH_SWITCH_DEBUG=ON so
    aniswitchStartupLog actually writes startup.log (switch_wrapper.c).
    Without DEBUG the app still runs stress but host sees no STRESS: lines.

    In-app stress (src/ui/nav_stress.cpp) log dialect:
      STRESS: START cycles=50 battery=14 (no player/onboarding)
      STRESS: BEGIN cycle 1/50 depth=1
      STRESS: open settings c=1 s=0 depth=1
      STRESS: opened settings depth=2
      STRESS: cycle 1/50 complete depth=1 fails=0
      STRESS: DONE cycles=50 fails=0 peakDepth=3
      STRESS: FAIL cycles=50 fails=2 peakDepth=4 endDepth=1
      STRESS: FAIL <step> ...          (intermediate; no cycles=)
      STRESS: WARN endDepth=2 expected 1

    Poll stops only on TERMINAL markers:
      STRESS: DONE cycles=   |   STRESS: FAIL cycles=
    Intermediate FAIL lines are counted but do not abort the run.
    Exit 0 only when DONE and terminal fails=0.

.EXAMPLE
    # One smoke round first (autotour "settings")
    powershell -ExecutionPolicy Bypass -File scripts\eden_stress_run.ps1 -Mode smoke

    # Then full 50-cycle navigation stress
    powershell -ExecutionPolicy Bypass -File scripts\eden_stress_run.ps1 -Mode stress50 -Cycles 50 -Force

    # Mini stress smoke (1 cycle) — exercises the stress battery quickly
    powershell -ExecutionPolicy Bypass -File scripts\eden_stress_run.ps1 -Mode stress50 -Cycles 1 -Force

.NOTES
    Exit codes:
      0  STRESS: DONE and fails=0  (or non-stress mode produced app log evidence)
      2  STRESS: FAIL (terminal) or fail_count>0
      3  timeout waiting for terminal DONE/FAIL
      4  setup error (Eden / NRO / paths)
      5  launched but no STRESS/AUTOTOUR evidence (manual title launch needed)
#>

[CmdletBinding()]
param(
    # Path to aniswitch.nro. Default: newest release/<ts>/aniswitch.nro,
    # else release/20260918-133828/aniswitch.nro.
    [string]$NroPath = '',

    # Autotour mode written to line1.
    [ValidateNotNullOrEmpty()]
    [string]$Mode = 'smoke',

    # Cycle count for stress modes (written to autotour line2).
    [int]$Cycles = 50,

    # Max seconds to poll. -1 => auto (stress ~14s/cycle + 180s; smoke 180s).
    [int]$TimeoutSec = -1,

    # Where to put screenshots + final log + summary. Relative to project root.
    [string]$OutDir = 'build_logs/stress',

    # Eden install root. Default = known path; else search E:\AI\**Eden*.
    [string]$EdenRoot = '',

    # Kill leftover eden/eden-cli processes before launch.
    [switch]$Force,

    # Skip starting Eden (deploy + autotour only).
    [switch]$NoLaunch,

    # Poll interval seconds for startup.log.
    [int]$PollSec = 2,

    # Snapshot schedule seconds after launch (end always captured too).
    [int[]]$ShotAtSec = @(15, 45, 90, 150, 240, 360),

    # Autotour mode used when -Mode smoke.
    [string]$SmokeMode = 'settings',

    # For -Mode online: episodeId subjectId written to autotour line2.
    [string]$OnlinePair = '',

    # Compat aliases with earlier draft param names.
    [switch]$KillExisting,
    [switch]$SkipLaunch
)

if ($KillExisting) { $Force = $true }
if ($SkipLaunch)   { $NoLaunch = $true }

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# ---------------------------------------------------------------------------
# Paths
# ---------------------------------------------------------------------------
$ProjectRoot = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
if (-not (Test-Path $ProjectRoot)) { $ProjectRoot = 'E:\AI\ani-switch' }

$DefaultEden = 'E:\AI\【Eden】- 0.2.1正式版最新整合版-（推荐）\Eden-v0.2.1-23.0\Eden-Windows-v0.2.1-pgo'
$FallbackNroRel = 'release/20260918-133828/aniswitch.nro'

function Write-Step([string]$msg, [string]$color = 'Cyan') {
    Write-Host "[$(Get-Date -Format 'HH:mm:ss')] $msg" -ForegroundColor $color
}

function Resolve-EdenRoot {
    param([string]$Hint)
    if ($Hint -and (Test-Path $Hint)) { return (Resolve-Path $Hint).Path }
    if (Test-Path $DefaultEden) { return $DefaultEden }
    if (Test-Path 'E:\AI') {
        $dirs = Get-ChildItem -Path 'E:\AI' -Directory -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -match 'Eden' }
        foreach ($d in $dirs) {
            $exe = Get-ChildItem -Path $d.FullName -Recurse -Include 'eden.exe','Eden.exe' -ErrorAction SilentlyContinue |
                Select-Object -First 1
            if ($exe) { return $exe.Directory.FullName }
        }
    }
    return $null
}

function Resolve-EdenExe {
    param([string]$Root)
    foreach ($name in @('eden-cli.exe', 'Eden-cli.exe', 'eden.exe', 'Eden.exe')) {
        $p = Join-Path $Root $name
        if (Test-Path $p) { return $p }
    }
    $found = Get-ChildItem -Path $Root -Recurse -Include 'eden*.exe','Eden*.exe' -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -notmatch 'unins|crash' } |
        Select-Object -First 5
    if ($found) {
        $cli = $found | Where-Object { $_.Name -match 'cli' } | Select-Object -First 1
        if ($cli) { return $cli.FullName }
        return $found[0].FullName
    }
    return $null
}

function Resolve-NroPath {
    param([string]$Hint, [string]$Root)
    if ($Hint) {
        if (Test-Path $Hint) { return (Resolve-Path $Hint).Path }
        throw "NRO not found: $Hint"
    }
    $relDir = Join-Path $Root 'release'
    $picked = $null
    if (Test-Path $relDir) {
        $dirs = Get-ChildItem -Path $relDir -Directory -ErrorAction SilentlyContinue |
            Where-Object { Test-Path (Join-Path $_.FullName 'aniswitch.nro') } |
            Sort-Object { $_.Name } -Descending
        if ($dirs) { $picked = Join-Path $dirs[0].FullName 'aniswitch.nro' }
    }
    if (-not $picked) {
        $fallback = Join-Path $Root $FallbackNroRel
        if (Test-Path $fallback) { $picked = $fallback }
    }
    if (-not $picked) {
        $rootNro = Join-Path $Root 'aniswitch.nro'
        if (Test-Path $rootNro) { $picked = $rootNro }
    }
    if (-not $picked -or -not (Test-Path $picked)) {
        throw "Could not resolve aniswitch.nro under $relDir (fallback $FallbackNroRel missing)"
    }
    return (Resolve-Path $picked).Path
}

function Write-AutotourFile {
    param([string]$Path, [string]$Line1, [string]$Line2)
    $dir = Split-Path -Parent $Path
    if (-not (Test-Path $dir)) { New-Item -ItemType Directory -Path $dir -Force | Out-Null }
    # ASCII bytes only — no BOM. CRLF ok (app strips CR).
    $text = if ($Line2 -and $Line2 -ne '') { "$Line1`r`n$Line2`r`n" } else { "$Line1`r`n" }
    $bytes = [System.Text.Encoding]::ASCII.GetBytes($text)
    [System.IO.File]::WriteAllBytes($Path, $bytes)
    Write-Step "autotour written: $Path  line1='$Line1' line2='$Line2'" 'DarkGray'
}

function Backup-Or-TruncateLog {
    param([string]$LogPath, [string]$OutDirAbs)
    if (Test-Path $LogPath) {
        $bak = Join-Path $OutDirAbs ("startup.prev." + (Get-Date -Format 'yyyyMMddHHmmss') + ".log")
        try {
            Copy-Item -Path $LogPath -Destination $bak -Force
            Write-Step "startup.log backed up -> $bak" 'DarkGray'
        } catch {
            Write-Step "startup.log backup failed: $($_.Exception.Message)" 'Yellow'
        }
        try {
            [System.IO.File]::WriteAllBytes($LogPath, [byte[]]@())
        } catch {
            Write-Step "startup.log truncate failed: $($_.Exception.Message)" 'Yellow'
        }
    } else {
        $dir = Split-Path -Parent $LogPath
        if (-not (Test-Path $dir)) { New-Item -ItemType Directory -Path $dir -Force | Out-Null }
        New-Item -ItemType File -Path $LogPath -Force | Out-Null
    }
}

# ---------------------------------------------------------------------------
# Screenshots — SetProcessDPIAware + GetWindowRect + CopyFromScreen
# Do NOT SendKeys into the game (known broken).
# ---------------------------------------------------------------------------
$script:Win32Ready = $false
$script:ShotPids = @()
function Init-Win32 {
    if ($script:Win32Ready) { return }
    $src = @'
using System;
using System.Runtime.InteropServices;
using System.Text;
public class EdenWin32 {
    [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
    [DllImport("user32.dll", CharSet=CharSet.Unicode)]
        public static extern IntPtr FindWindowW(string lpClassName, string lpWindowName);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hWnd, out RECT lpRect);
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr hWnd);
    [DllImport("user32.dll")] public static extern bool EnumWindows(EnumWindowsProc lpEnumFunc, IntPtr lParam);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)]
        public static extern int GetWindowTextW(IntPtr hWnd, StringBuilder lpString, int nMaxCount);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint lpdwProcessId);
    public delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);
    [StructLayout(LayoutKind.Sequential)]
    public struct RECT { public int Left, Top, Right, Bottom; }
}
'@
    try {
        Add-Type -TypeDefinition $src -ErrorAction Stop
    } catch {
        if ($_.Exception.Message -notmatch 'already exists|already defined') { throw }
    }
    try { [void][EdenWin32]::SetProcessDPIAware() } catch {}
    $script:Win32Ready = $true
}

function Find-EdenWindow {
    param([int[]]$ProcessIds)
    Init-Win32
    $script:ShotPids = @()
    if ($ProcessIds) { $script:ShotPids = @($ProcessIds) }
    $hits = New-Object System.Collections.ArrayList
    $cb = [EdenWin32+EnumWindowsProc]{
        param([IntPtr]$hWnd, [IntPtr]$lParam)
        if (-not [EdenWin32]::IsWindowVisible($hWnd)) { return $true }
        $sb = New-Object System.Text.StringBuilder 512
        [void][EdenWin32]::GetWindowTextW($hWnd, $sb, $sb.Capacity)
        $title = $sb.ToString()
        if ([string]::IsNullOrWhiteSpace($title)) { return $true }
        $pidOut = [uint32]0
        [void][EdenWin32]::GetWindowThreadProcessId($hWnd, [ref]$pidOut)
        $matchTitle = ($title -match 'Eden|aniswitch|ani-switch')
        $matchPid = $false
        if ($script:ShotPids -and $script:ShotPids.Count -gt 0) {
            $matchPid = ($script:ShotPids -contains [int]$pidOut)
        }
        if ($matchTitle -or $matchPid) {
            [void]$hits.Add([pscustomobject]@{ Hwnd = $hWnd; Title = $title; Pid = [int]$pidOut })
        }
        return $true
    }
    [void][EdenWin32]::EnumWindows($cb, [IntPtr]::Zero)
    $best = $null
    foreach ($h in $hits) {
        if ($h.Title -match 'Eden') { $best = $h; break }
    }
    if (-not $best -and $hits.Count -gt 0) { $best = $hits[0] }
    return $best
}

function Capture-Screenshot {
    param([string]$OutPath, [int[]]$ProcessIds)
    try {
        Add-Type -AssemblyName System.Drawing -ErrorAction SilentlyContinue
        Init-Win32
        $win = Find-EdenWindow -ProcessIds $ProcessIds
        if (-not $win) {
            Write-Step "screenshot skip: no Eden window" 'Yellow'
            return $false
        }
        $rect = New-Object EdenWin32+RECT
        if (-not [EdenWin32]::GetWindowRect($win.Hwnd, [ref]$rect)) {
            Write-Step "screenshot skip: GetWindowRect failed for '$($win.Title)'" 'Yellow'
            return $false
        }
        $w = [Math]::Max(1, $rect.Right - $rect.Left)
        $h = [Math]::Max(1, $rect.Bottom - $rect.Top)
        if ($w -lt 32 -or $h -lt 32) {
            Write-Step "screenshot skip: window too small ${w}x${h}" 'Yellow'
            return $false
        }
        $bmp = New-Object System.Drawing.Bitmap($w, $h)
        $g = [System.Drawing.Graphics]::FromImage($bmp)
        try {
            $g.CopyFromScreen($rect.Left, $rect.Top, 0, 0, (New-Object System.Drawing.Size($w, $h)))
        } finally {
            $g.Dispose()
        }
        $dir = Split-Path -Parent $OutPath
        if (-not (Test-Path $dir)) { New-Item -ItemType Directory -Path $dir -Force | Out-Null }
        $bmp.Save($OutPath, [System.Drawing.Imaging.ImageFormat]::Png)
        $bmp.Dispose()
        Write-Step "shot: $OutPath  ($($win.Title) ${w}x${h})" 'DarkGray'
        return $true
    } catch {
        Write-Step "screenshot error: $($_.Exception.Message)" 'Yellow'
        return $false
    }
}

# ---------------------------------------------------------------------------
# STRESS parser — matches src/ui/nav_stress.cpp
# ---------------------------------------------------------------------------
function Get-StressSummary {
    param([string[]]$Lines)
    $summary = [pscustomobject]@{
        done               = $false
        fail               = $false
        terminal_fail      = $false
        started            = $false
        cycles_requested   = $null
        cycles_seen        = 0
        fail_count         = 0
        max_depth          = 0
        peak_depth         = $null
        end_depth          = $null
        fail_markers       = 0
        open_count         = 0
        keyB_count         = 0
        stress_lines       = 0
        last_line          = ''
        battery_note       = ''
        has_terminal_fails = $false
        cycle_rows         = @()
    }
    foreach ($ln in $Lines) {
        if ($null -eq $ln) { continue }
        if ($ln -notmatch 'STRESS:') { continue }
        $summary.stress_lines++
        $summary.last_line = $ln

        if ($ln -match 'STRESS:\s*DONE') { $summary.done = $true }
        if ($ln -match 'STRESS:\s*FAIL') {
            $summary.fail = $true
            $summary.fail_markers++
        }
        if ($ln -match 'STRESS:\s*FAIL\s+cycles=') { $summary.terminal_fail = $true }

        if ($ln -match 'STRESS:\s*START\s+cycles=(\d+)') {
            $summary.started = $true
            $summary.cycles_requested = [int]$Matches[1]
        }
        if ($ln -match 'battery=(\d+)') { $summary.battery_note = "battery=$($Matches[1])" }

        if ($ln -match 'STRESS:\s*open\s+') { $summary.open_count++ }
        if ($ln -match 'STRESS:\s*key B') { $summary.keyB_count++ }

        $isTerm = ($ln -match 'STRESS:\s*DONE' -or $ln -match 'STRESS:\s*FAIL\s+cycles=')
        if ($ln -match 'cycles=(\d+)') {
            $c = [int]$Matches[1]
            if ($null -eq $summary.cycles_requested) { $summary.cycles_requested = $c }
            if ($isTerm -and $c -gt $summary.cycles_seen) { $summary.cycles_seen = $c }
        }

        $isCompleteLine = ($ln -match 'cycle\s+\d+/\d+\s+complete')
        if ($ln -match 'fails=(\d+)') {
            $f = [int]$Matches[1]
            if ($isTerm) {
                $summary.fail_count = $f
                $summary.has_terminal_fails = $true
            } elseif ($isCompleteLine) {
                if (-not $summary.has_terminal_fails -and $f -gt $summary.fail_count) {
                    $summary.fail_count = $f
                }
            } elseif ($f -gt $summary.fail_count) {
                if (-not $summary.has_terminal_fails) { $summary.fail_count = $f }
            }
        }
        if ($ln -match 'peakDepth=(\d+)') { $summary.peak_depth = [int]$Matches[1] }
        if ($ln -match 'endDepth=(\d+)') { $summary.end_depth = [int]$Matches[1] }

        $depth = $null
        if ($ln -match '\bdepth\s*=\s*(\d+)') { $depth = [int]$Matches[1] }
        if ($null -ne $depth -and $depth -gt $summary.max_depth) {
            $summary.max_depth = $depth
        }

        $cycNum = $null
        $cycTot = $null
        if ($ln -match 'cycle\s+(\d+)/(\d+)') {
            $cycNum = [int]$Matches[1]
            $cycTot = [int]$Matches[2]
            if ($null -eq $summary.cycles_requested) { $summary.cycles_requested = $cycTot }
        }
        if ($null -ne $cycNum) {
            if ($cycNum -gt $summary.cycles_seen) { $summary.cycles_seen = $cycNum }
            $isComplete = ($ln -match 'complete')
            $rowFail = $false
            $rowFails = $null
            if ($ln -match 'fails=(\d+)') {
                $rowFails = [int]$Matches[1]
                $rowFail = ($rowFails -gt 0)
            } elseif ($ln -match 'FAIL') {
                $rowFail = $true
            }
            if ($isComplete -or $ln -match 'BEGIN|FAIL|DONE') {
                $kind = 'other'
                if ($isComplete) { $kind = 'complete' }
                elseif ($ln -match 'BEGIN') { $kind = 'begin' }
                elseif ($ln -match 'DONE') { $kind = 'done' }
                elseif ($ln -match 'FAIL') { $kind = 'fail' }
                $summary.cycle_rows += [pscustomobject]@{
                    cycle = $cycNum
                    total = $cycTot
                    depth = $depth
                    peak  = $summary.peak_depth
                    fails = $rowFails
                    fail  = $rowFail
                    kind  = $kind
                    raw   = $ln
                }
            }
        }
    }

    if ($null -ne $summary.peak_depth -and $summary.peak_depth -gt $summary.max_depth) {
        $summary.max_depth = $summary.peak_depth
    } elseif ($null -eq $summary.peak_depth -and $summary.max_depth -gt 0) {
        $summary.peak_depth = $summary.max_depth
    }
    if (-not $summary.has_terminal_fails -and $summary.fail_markers -gt 0) {
        if ($summary.fail_count -eq 0) { $summary.fail_count = $summary.fail_markers }
    }
    return $summary
}

function Write-SummaryArtifacts {
    param(
        [string]$OutDir,
        [object]$Summary,
        [string[]]$StressLines,
        [hashtable]$Meta
    )
    $stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
    $csvPath = Join-Path $OutDir "stress-summary-$stamp.csv"
    $jsonPath = Join-Path $OutDir "stress-summary-$stamp.json"
    $csvStable = Join-Path $OutDir ("summary.$($Meta.mode).csv")
    $jsonStable = Join-Path $OutDir ("summary.$($Meta.mode).json")

    $rows = @($Summary.cycle_rows)
    if ($rows.Count -gt 0) {
        $rows | Select-Object cycle, total, depth, peak, fails, fail, kind, raw |
            Export-Csv -Path $csvPath -NoTypeInformation -Encoding UTF8
    } else {
        @([pscustomobject]@{ cycle=''; total=''; depth=''; peak=''; fails=''; fail=''; kind=''; raw=$Summary.last_line }) |
            Export-Csv -Path $csvPath -NoTypeInformation -Encoding UTF8
    }
    Copy-Item $csvPath $csvStable -Force

    $json = [ordered]@{
        generated_utc = (Get-Date).ToUniversalTime().ToString('o')
        meta          = $Meta
        summary       = @{
            done               = $Summary.done
            fail               = $Summary.fail
            terminal_fail      = $Summary.terminal_fail
            started            = $Summary.started
            cycles_requested   = $Summary.cycles_requested
            cycles_seen        = $Summary.cycles_seen
            fail_count         = $Summary.fail_count
            peak_depth         = $Summary.peak_depth
            max_depth          = $Summary.max_depth
            end_depth          = $Summary.end_depth
            fail_markers       = $Summary.fail_markers
            open_count         = $Summary.open_count
            keyB_count         = $Summary.keyB_count
            stress_lines       = $Summary.stress_lines
            last_line          = $Summary.last_line
            battery_note       = $Summary.battery_note
        }
        stress_lines  = $StressLines
    }
    $jsonTxt = $json | ConvertTo-Json -Depth 6
    $jsonTxt | Set-Content -Path $jsonPath -Encoding UTF8
    $jsonTxt | Set-Content -Path $jsonStable -Encoding UTF8
    Write-Step "summary CSV  : $csvPath (also $(Split-Path -Leaf $csvStable))"
    Write-Step "summary JSON : $jsonPath (also $(Split-Path -Leaf $jsonStable))"
    return @{ Csv = $csvPath; Json = $jsonPath; CsvStable = $csvStable; JsonStable = $jsonStable }
}

# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------
$outDirAbs = $OutDir
if (-not [System.IO.Path]::IsPathRooted($outDirAbs)) {
    $outDirAbs = Join-Path $ProjectRoot $OutDir
}
New-Item -ItemType Directory -Path $outDirAbs -Force | Out-Null

Write-Step "=== eden_stress_run  mode=$Mode cycles=$Cycles ==="

# 1) Eden root + exe
if (-not $EdenRoot) { $EdenRoot = Resolve-EdenRoot -Hint '' }
if (-not $EdenRoot -or -not (Test-Path $EdenRoot)) {
    Write-Step "Eden root not found (tried default + E:\\AI\\*Eden* search)" 'Red'
    exit 4
}
$edenExe = Resolve-EdenExe -Root $EdenRoot
if (-not $edenExe) {
    Write-Step "No eden*.exe under $EdenRoot" 'Red'
    exit 4
}
$edenCli = Join-Path $EdenRoot 'eden-cli.exe'
$edenGui = Join-Path $EdenRoot 'eden.exe'
if (-not (Test-Path $edenCli)) { $edenCli = $null }
if (-not (Test-Path $edenGui)) { $edenGui = $edenExe }

$sdmcAni        = Join-Path $EdenRoot 'user\sdmc\switch\aniswitch'
$sdmcNroNested  = Join-Path $sdmcAni 'aniswitch.nro'
$sdmcNroFlat    = Join-Path $EdenRoot 'user\sdmc\switch\aniswitch.nro'
$sdmcNacpNested = Join-Path $sdmcAni 'aniswitch.nacp'
$sdmcNacpFlat   = Join-Path $EdenRoot 'user\sdmc\switch\aniswitch.nacp'
$autotourPath   = Join-Path $sdmcAni 'autotour'
$startupLog     = Join-Path $sdmcAni 'startup.log'

Write-Step "Eden root : $EdenRoot"
Write-Step "eden-cli  : $(if ($edenCli) { $edenCli } else { '(none)' })"
Write-Step "eden.exe  : $(if (Test-Path $edenGui) { $edenGui } else { '(none)' })"
Write-Step "NOTE: NRO must be ANISWITCH_SWITCH_DEBUG=ON for startup.log STRESS lines" 'Yellow'

# 2) Kill leftovers
if ($Force) {
    Write-Step "Force: killing eden / eden-cli processes"
    Get-Process -Name 'eden','Eden','eden-cli','Eden-cli' -ErrorAction SilentlyContinue |
        ForEach-Object {
            Write-Step "  stop $($_.ProcessName) pid=$($_.Id)" 'DarkGray'
            try { $_.Kill() } catch {}
        }
    Start-Sleep -Milliseconds 500
}

# 3) Resolve + copy NRO (both sdmc locations)
$nroSrc = Resolve-NroPath -Hint $NroPath -Root $ProjectRoot
$nroHash = (Get-FileHash -Path $nroSrc -Algorithm SHA256).Hash
$nroLen = (Get-Item $nroSrc).Length
Write-Step "NRO src   : $nroSrc  ($nroLen bytes, SHA256 $($nroHash.Substring(0,16))...)"

New-Item -ItemType Directory -Path $sdmcAni -Force | Out-Null
Copy-Item -Path $nroSrc -Destination $sdmcNroNested -Force
Copy-Item -Path $nroSrc -Destination $sdmcNroFlat -Force
$srcDir = Split-Path -Parent $nroSrc
$nacpSrc = Join-Path $srcDir 'aniswitch.nacp'
if (Test-Path $nacpSrc) {
    Copy-Item -Path $nacpSrc -Destination $sdmcNacpNested -Force
    Copy-Item -Path $nacpSrc -Destination $sdmcNacpFlat -Force -ErrorAction SilentlyContinue
}
$caSrc = Join-Path $srcDir 'ca-bundle.crt'
if (Test-Path $caSrc) {
    Copy-Item -Path $caSrc -Destination (Join-Path $sdmcAni 'ca-bundle.crt') -Force
}
$srcJson = Join-Path (Split-Path $srcDir -Parent) 'dist\sd-aniswitch\sources.json'
if (-not (Test-Path $srcJson)) {
    $srcJson = Join-Path $PSScriptRoot '..\dist\sd-aniswitch\sources.json'
}
if (Test-Path $srcJson) {
    Copy-Item -Path $srcJson -Destination (Join-Path $sdmcAni 'sources.json') -Force
    Write-Step "sources.json deployed"
}
Write-Step "NRO deployed -> $sdmcNroNested + $sdmcNroFlat"

# 4) Autotour mode mapping
$modeLower = $Mode.ToLowerInvariant()
$autotourMode = $modeLower
$autotourLine2 = ''
$isStress = $false

switch -Regex ($modeLower) {
    '^stress\d*$' {
        $isStress = $true
        $autotourMode = $modeLower
        $autotourLine2 = "$Cycles"
        if ($modeLower -eq 'stress') { $autotourMode = "stress$Cycles" }
        if ($TimeoutSec -lt 0) {
            # ~14s/cycle: 14-step battery (350+200+180ms delays) + settle + 300ms cycle gap
            $TimeoutSec = [int][Math]::Max(600, [Math]::Ceiling($Cycles * 14) + 180)
        }
    }
    '^smoke$' {
        $autotourMode = $SmokeMode
        $autotourLine2 = ''
        if ($TimeoutSec -lt 0) { $TimeoutSec = 180 }
        if ($Cycles -gt 1) { $Cycles = 1 }
    }
    '^online$' {
        $autotourMode = 'online'
        $autotourLine2 = $OnlinePair
        if ($TimeoutSec -lt 0) { $TimeoutSec = 180 }
        $isStress = $true  # keep polling; do not early-exit on AUTOTOUR open
    }
    '^playtest$' {
        $autotourMode = 'playtest'
        $autotourLine2 = $OnlinePair
        if ($TimeoutSec -lt 0) { $TimeoutSec = 180 }
        $isStress = $true
    }
    '^(playtestd|playtest-direct)$' {
        $autotourMode = 'playtestd'
        $autotourLine2 = $OnlinePair
        if ($TimeoutSec -lt 0) { $TimeoutSec = 180 }
        $isStress = $true
    }
    default {
        $autotourMode = $modeLower
        $autotourLine2 = ''
        if ($TimeoutSec -lt 0) { $TimeoutSec = 180 }
    }
}
if ($TimeoutSec -lt 0) { $TimeoutSec = 600 }

Write-AutotourFile -Path $autotourPath -Line1 $autotourMode -Line2 $autotourLine2

# 5) Truncate / backup startup.log
Backup-Or-TruncateLog -LogPath $startupLog -OutDirAbs $outDirAbs

# 6) Launch Eden
$launchMethod = 'none'
$launchArgs = ''
$launchedProc = $null

if (-not $NoLaunch) {
    $nroArgsCandidates = @("`"$sdmcNroNested`"", "`"$sdmcNroFlat`"", "`"$nroSrc`"")
    $exeCandidates = @()
    if ($edenCli) { $exeCandidates += $edenCli }
    if ($edenGui) { $exeCandidates += $edenGui }

    $started = $false
    foreach ($exe in $exeCandidates) {
        if ($started) { break }
        foreach ($arg in $nroArgsCandidates) {
            Write-Step "try launch: `"$exe`" $arg"
            try {
                $launchedProc = Start-Process -FilePath $exe -ArgumentList $arg -PassThru -WorkingDirectory $EdenRoot
                Start-Sleep -Seconds 2
                if ($launchedProc -and -not $launchedProc.HasExited) {
                    $started = $true
                    $leaf = Split-Path -Leaf $exe
                    $launchMethod = "$leaf + nro-arg"
                    $launchArgs = $arg
                    Write-Step "launched pid=$($launchedProc.Id) via $launchMethod $launchArgs" 'Green'
                    break
                } else {
                    $code = $null
                    try { $code = $launchedProc.ExitCode } catch {}
                    Write-Step "  exited immediately (code=$code) — try next" 'Yellow'
                }
            } catch {
                Write-Step "  start failed: $($_.Exception.Message)" 'Yellow'
            }
        }
    }

    if (-not $started) {
        $gui = if ($edenGui -and (Test-Path $edenGui)) { $edenGui } else { $edenExe }
        Write-Step "CLI launch did not stick; starting GUI: $gui" 'Yellow'
        Write-Step "USER ACTION: launch the aniswitch homebrew title in Eden if it does not auto-start." 'Yellow'
        try {
            $launchedProc = Start-Process -FilePath $gui -PassThru -WorkingDirectory $EdenRoot
            $launchMethod = 'gui-manual'
            $started = $true
        } catch {
            Write-Step "GUI start failed: $($_.Exception.Message)" 'Red'
        }
    }

    if (-not $started) {
        Write-Step "Failed to start Eden" 'Red'
        exit 4
    }
} else {
    Write-Step "NoLaunch: deploy + autotour only. Start aniswitch in Eden yourself." 'Yellow'
    $launchMethod = 'nolaunch'
}

$edenPids = @(Get-Process -Name 'eden','Eden','eden-cli','Eden-cli' -ErrorAction SilentlyContinue |
    Select-Object -ExpandProperty Id)
if ($launchedProc) {
    try {
        if ($edenPids -notcontains $launchedProc.Id) { $edenPids += $launchedProc.Id }
    } catch {}
}

# 7) Poll + 8) screenshots
Write-Step "polling startup.log every ${PollSec}s for up to ${TimeoutSec}s"
Write-Step "  terminal markers: STRESS: DONE cycles= | STRESS: FAIL cycles="
$shotDir = Join-Path $outDirAbs "shots-$(Get-Date -Format 'yyyyMMdd-HHmmss')"
New-Item -ItemType Directory -Path $shotDir -Force | Out-Null

$sw = [System.Diagnostics.Stopwatch]::StartNew()
$shotsDone = New-Object 'System.Collections.Generic.HashSet[int]'
$shotTimes = @($ShotAtSec | Where-Object { $_ -gt 0 } | Sort-Object -Unique)
if ($shotTimes.Count -gt 6) { $shotTimes = @($shotTimes[0..5]) }

$logText = ''
$stressLines = @()
$done = $false
$failed = $false
$terminalFail = $false
$exitCode = 3

while ($sw.Elapsed.TotalSeconds -lt $TimeoutSec) {
    $elapsed = [int][Math]::Floor($sw.Elapsed.TotalSeconds)

    foreach ($t in $shotTimes) {
        if ($elapsed -ge $t -and -not $shotsDone.Contains($t)) {
            [void]$shotsDone.Add($t)
            $png = Join-Path $shotDir ("t{0:D4}s.png" -f $t)
            [void](Capture-Screenshot -OutPath $png -ProcessIds $edenPids)
        }
    }

    if (Test-Path $startupLog) {
        try { $logText = [System.IO.File]::ReadAllText($startupLog) }
        catch {
            try { $logText = Get-Content -Path $startupLog -Raw -ErrorAction Stop } catch { }
        }
    }
    $lines = @()
    if ($logText) { $lines = $logText -split "`r?`n" }
    $stressLines = @($lines | Where-Object { $_ -match 'STRESS:' })

    if ($logText -match 'STRESS:\s*DONE') { $done = $true }
    if ($logText -match 'STRESS:\s*FAIL') { $failed = $true }
    if ($logText -match 'STRESS:\s*FAIL\s+cycles=') { $terminalFail = $true }
    $autotourArmed = ($logText -match 'AUTOTOUR:')
    $stressStarted = ($logText -match 'STRESS:\s*START')

    # Only stop on TERMINAL markers so intermediate FAIL steps do not abort.
    if ($logText -match 'STRESS:\s*DONE\s+cycles=' -or $terminalFail) {
        if ($logText -match 'STRESS:\s*DONE\s+cycles=') { $done = $true }
        Write-Step "terminal marker at t=${elapsed}s  DONE=$done FAIL=$terminalFail" 'Green'
        break
    }

    # Online play smoke: wait for real playback breadcrumbs (not just AUTOTOUR).
    if ($modeLower -eq 'online' -or $modeLower -eq 'playtest' -or $modeLower -eq 'playtestd' -or $modeLower -eq 'playtest-direct') {
        if ($logText -match 'PLAYTEST:\s*DONE\s+ep=') {
            $done = $true
            Write-Step "playtest DONE at t=${elapsed}s" 'Green'
            break
        }
        if ($logText -match 'PLAYTEST:\s*FAIL') {
            $failed = $true
        }
        if ($logText -match 'player:\s*file loaded') {
            # online path success marker when not playtest
            if ($modeLower -eq 'online') {
                $done = $true
                Write-Step "online play: file loaded at t=${elapsed}s" 'Green'
                break
            }
        }
        if ($logText -match 'player:\s*hls playlist fail|player:\s*dl status=|解析失败') {
            $failed = $true
        }
    }

    # Smoke / screen modes that never emit STRESS: succeed once AUTOTOUR opened
    if (-not $isStress -and $autotourArmed -and $elapsed -ge 12 -and $modeLower -ne 'online') {
        if ($logText -match 'AUTOTOUR: open') {
            Write-Step "smoke AUTOTOUR: open seen at t=${elapsed}s" 'Green'
            $done = $true
            break
        }
    }

    if ($elapsed -gt 0 -and ($elapsed % 15) -eq 0) {
        $nStress = $stressLines.Count
        $nFailM = @($stressLines | Where-Object { $_ -match 'STRESS:\s*FAIL' }).Count
        Write-Step "  t=${elapsed}s stress_lines=$nStress fail_markers=$nFailM started=$stressStarted autotour=$autotourArmed" 'DarkGray'
    }

    Start-Sleep -Seconds $PollSec
}
$sw.Stop()

$finalPng = Join-Path $shotDir 't_end.png'
[void](Capture-Screenshot -OutPath $finalPng -ProcessIds $edenPids)

if (Test-Path $startupLog) {
    try { $logText = [System.IO.File]::ReadAllText($startupLog) } catch {}
}
$allLines = @()
if ($logText) { $allLines = $logText -split "`r?`n" }
$stressLines = @($allLines | Where-Object { $_ -match 'STRESS:' })
if ($logText -match 'STRESS:\s*DONE') { $done = $true }
if ($logText -match 'STRESS:\s*FAIL') { $failed = $true }
if ($logText -match 'STRESS:\s*FAIL\s+cycles=') { $terminalFail = $true }

# 9) Copy final log + summary
$logCopy = Join-Path $outDirAbs "startup.$Mode.log"
if (Test-Path $startupLog) {
    Copy-Item -Path $startupLog -Destination $logCopy -Force
    Write-Step "startup.log copied -> $logCopy"
} else {
    Write-Step "startup.log missing at $startupLog" 'Yellow'
}

$stressTxt = Join-Path $outDirAbs "stress-lines-$(Get-Date -Format 'yyyyMMdd-HHmmss').txt"
$stressLines | Set-Content -Path $stressTxt -Encoding UTF8
Write-Step "STRESS lines ($($stressLines.Count)) -> $stressTxt"

$summary = Get-StressSummary -Lines $stressLines

$meta = @{
    mode            = $Mode
    autotour_mode   = $autotourMode
    autotour_line2  = $autotourLine2
    cycles          = $Cycles
    timeout_sec     = $TimeoutSec
    elapsed_sec     = [int]$sw.Elapsed.TotalSeconds
    nro_src         = $nroSrc
    nro_sha256      = $nroHash
    nro_deploy      = $sdmcNroNested
    nro_deploy_flat = $sdmcNroFlat
    eden_root       = $EdenRoot
    launch_method   = $launchMethod
    launch_args     = $launchArgs
    shot_dir        = $shotDir
    out_dir         = $outDirAbs
    startup_log     = $logCopy
}

$arts = Write-SummaryArtifacts -OutDir $outDirAbs -Summary $summary -StressLines $stressLines -Meta $meta
$meta.summary_csv = $arts.Csv
$meta.summary_json = $arts.Json
$shotNames = @()
Get-ChildItem -Path $shotDir -Filter '*.png' -ErrorAction SilentlyContinue | ForEach-Object { $shotNames += $_.Name }
$meta.shots = $shotNames

Write-Host ''
Write-Step '=== RESULT ===' 'White'
Write-Host "  mode          : $Mode (autotour '$autotourMode' line2='$autotourLine2')"
Write-Host "  launch        : $launchMethod"
Write-Host "  elapsed       : $([int]$sw.Elapsed.TotalSeconds)s / ${TimeoutSec}s"
Write-Host "  stress lines  : $($stressLines.Count)"
Write-Host "  START         : $($summary.started)"
Write-Host "  DONE          : $done"
Write-Host "  FAIL(any)     : $failed"
Write-Host "  FAIL(terminal): $terminalFail"
Write-Host "  cycles_req    : $($summary.cycles_requested)"
Write-Host "  cycles_seen   : $($summary.cycles_seen)"
Write-Host "  fail_count    : $($summary.fail_count)"
Write-Host "  peakDepth     : $($summary.peak_depth)"
Write-Host "  max_depth     : $($summary.max_depth)"
Write-Host "  end_depth     : $($summary.end_depth)"
Write-Host "  open_count    : $($summary.open_count)"
Write-Host "  last_line     : $($summary.last_line)"
Write-Host "  shots         : $shotDir"
Write-Host "  out           : $outDirAbs"
if ($launchMethod -eq 'gui-manual' -or $launchMethod -eq 'nolaunch') {
    Write-Host ''
    Write-Host '  NOTE: if startup.log has no AUTOTOUR:/STRESS: lines, Eden did not' -ForegroundColor Yellow
    Write-Host '  auto-start aniswitch. Launch the title manually in the Eden GUI.' -ForegroundColor Yellow
}

# 10) Exit codes
if ($isStress) {
    if ($summary.done -and -not $summary.terminal_fail -and $summary.fail_count -eq 0) {
        $exitCode = 0
    } elseif ($terminalFail -or $summary.fail_count -gt 0) {
        $exitCode = 2
    } elseif ($failed -and -not $done) {
        $exitCode = 2
    } else {
        $exitCode = 3
    }
} else {
    $appEvidence = ($logText -match 'AUTOTOUR:|main:|player:|HTTP:|STRESS:')
    if ($terminalFail -or ($failed -and $summary.fail_count -gt 0)) { $exitCode = 2 }
    elseif ($done) { if ($summary.fail_count -eq 0) { $exitCode = 0 } else { $exitCode = 2 } }
    elseif ($appEvidence) { $exitCode = 0 }
    elseif ($sw.Elapsed.TotalSeconds -ge $TimeoutSec) { $exitCode = 3 }
    else { $exitCode = 5 }
}

Write-Host ''
Write-Host "exit=$exitCode" -ForegroundColor $(if ($exitCode -eq 0) { 'Green' } else { 'Yellow' })
exit $exitCode
