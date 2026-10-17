# SPDX-FileCopyrightText: 2026 twinelink contributors
#
# SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
<#
.SYNOPSIS
Measures startup phases from the per-line startup trace.

.DESCRIPTION
Runs a desktop binary with TWINELINK_STARTUP_TRACE_FILE set, stops it as soon as
the phase of interest has been recorded, and prints the process age (proc_ms)
for that phase. Use -Runs to see how much a number moves between runs: a single
run is not a measurement.

The binaries spawn their peer processes (the daemon starts the tray, the tray
starts the daemon), which write to the same trace file. Lines are filtered by
the pid of the process that was started, and only processes started from this
build directory are stopped again - a KDE Connect installation elsewhere on the
machine is left alone.

The measured binary is started with a visible window: a hidden UI process never
presents a frame, so its "first frame presented" phase would never be recorded.

.PARAMETER Binary
kdeconnectd, kdeconnect-app, kdeconnect-indicator, or a path to an executable.

.PARAMETER Phase
Phase name to read, exactly as it appears in the trace. By default the script
waits until the trace goes quiet and reports the last startup phase.

.PARAMETER Runs
Number of runs. Defaults to 3.

.PARAMETER QuietMilliseconds
When no -Phase is given: how long the trace has to stay unchanged before the
last recorded phase is reported. Defaults to 4000 ms, which has to cover the gap
while the QML module loads.

.EXAMPLE
.\tools\measure-startup.ps1 -Binary kdeconnectd -Phase 'daemon: DBus service registered (org.kde.kdeconnect)'

.EXAMPLE
.\tools\measure-startup.ps1 -Binary kdeconnect-app -Phase 'app: first frame presented'
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$Binary,

    [string]$Phase,

    [int]$Runs = 3,

    [int]$TimeoutSeconds = 30,

    [string]$CraftRoot = 'C:\CraftRoot',

    [int]$QuietMilliseconds = 4000,

    [switch]$KeepTrace
)

$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$binDirectory = Join-Path $repoRoot 'build\bin'
$executable = if (Test-Path $Binary) { $Binary } else { Join-Path $binDirectory "$Binary.exe" }
if (-not (Test-Path $executable)) {
    throw "Executable not found: $executable (build the project first)"
}
if (Test-Path $CraftRoot) {
    $env:PATH = "$CraftRoot\bin;$env:PATH"
}

$processName = [IO.Path]::GetFileNameWithoutExtension($executable)
$traceDirectory = Join-Path ([IO.Path]::GetTempPath()) 'twinelink-startup'
New-Item -ItemType Directory -Force -Path $traceDirectory | Out-Null
$traceFile = Join-Path $traceDirectory ("{0}-{1}.log" -f $processName, [IO.Path]::GetRandomFileName())

# Only binaries started from this build directory are ours to stop.
function Get-OwnProcesses
{
    Get-Process -ErrorAction SilentlyContinue |
        Where-Object { $_.Path -and $_.Path.StartsWith($binDirectory, [StringComparison]::OrdinalIgnoreCase) }
}

$measurements = @()
for ($run = 1; $run -le $Runs; $run++) {
    # Start from a clean slate: a leftover instance of the same binary would take
    # over the single-instance service and never record its phases itself.
    Get-OwnProcesses | Stop-Process -Force -ErrorAction SilentlyContinue
    Start-Sleep -Milliseconds 500

    Remove-Item $traceFile -ErrorAction SilentlyContinue
    $env:TWINELINK_STARTUP_TRACE = '1'
    $env:TWINELINK_STARTUP_TRACE_FILE = $traceFile

    # The window must actually be shown, otherwise the UI process never presents
    # a frame and "first frame presented" is never recorded.
    $process = Start-Process -FilePath $executable -PassThru
    $processId = $process.Id

    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    $match = $null
    $own = @()
    $seen = -1
    $quietSince = Get-Date
    while (-not $match -and (Get-Date) -lt $deadline) {
        Start-Sleep -Milliseconds 50
        if (-not (Test-Path $traceFile)) {
            continue
        }
        # Filter by pid, not by name: the daemon spawns the tray and the tray
        # spawns the daemon, so other processes of the same name write here too.
        $own = @(Get-Content $traceFile -ErrorAction SilentlyContinue | Where-Object { $_ -match " pid=$processId " })
        if ($Phase) {
            $match = $own | Where-Object { $_ -match "phase=`"$([regex]::Escape($Phase))`"" } | Select-Object -Last 1
            continue
        }
        # Without a phase, wait for the trace to go quiet: the last line written
        # before the process settles down is the last startup phase. The window
        # has to be longer than the longest gap between phases - loading the QML
        # module takes seconds on its own.
        if ($own.Count -ne $seen) {
            $seen = $own.Count
            $quietSince = Get-Date
        } elseif ($own.Count -gt 0 -and (Get-Date).Subtract($quietSince).TotalMilliseconds -gt $QuietMilliseconds) {
            $match = $own[-1]
        }
    }

    if (-not $match) {
        Write-Warning "Run ${run}: nothing recorded for phase '$Phase' within $TimeoutSeconds s"
    } else {
        $value = [regex]::Match($match, 'proc_ms=(\d+|n/a)').Groups[1].Value
        $recordedPhase = [regex]::Match($match, 'phase="(.*)"').Groups[1].Value
        Write-Host ("run {0}: {1,6} ms  {2}" -f $run, $value, $recordedPhase)
        if ($value -match '^\d+$') {
            $measurements += [int]$value
        }
    }

    Get-OwnProcesses | Stop-Process -Force -ErrorAction SilentlyContinue
    Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
    Start-Sleep -Milliseconds 300
}

if ($measurements.Count -gt 1) {
    $sorted = $measurements | Sort-Object
    $median = $sorted[[int][Math]::Floor($sorted.Count / 2)]
    Write-Host ("min {0} ms / median {1} ms / max {2} ms over {3} runs" -f $sorted[0], $median, $sorted[-1], $measurements.Count)
}

if ($KeepTrace) {
    Write-Host "trace kept at $traceFile"
} else {
    Remove-Item $traceFile -ErrorAction SilentlyContinue
}
