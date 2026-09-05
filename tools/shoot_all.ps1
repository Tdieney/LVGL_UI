[CmdletBinding()]
param(
    # 'demo'      = demo telemetry on, motor STOPPED (matches the docs reference shots)
    # 'demo-run'  = demo on + motor spinning ~83% max RPM (livelier review shots)
    # 'none'      = no demo: disconnected telemetry (all-zero readouts)
    [ValidateSet('demo', 'demo-run', 'none')]
    [string]$Telemetry = 'demo',
    [ValidateRange(1, 600000)]
    [int]$Ms = 10000,
    [string]$OutDir = '',
    [string]$BuildDir = '',
    [switch]$SkipBuild
)

# One-command visual + heap review for all six tabs.
#
# Runs sim_pc.exe --shot for every tab, converts each RGB565 dump to a PNG
# via raw2png.py, and writes one heap_summary.txt with the LVGL heap report
# each shot printed. Default output is docs\screenshots (the documented
# reference shots live there); pass -OutDir to keep a working copy instead.
#
# Exit code is non-zero on any failed tab, so the .bat wrapper can be used in
# CI or a pre-commit check.

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))

if (-not $BuildDir) {
    $BuildDir = Join-Path $repoRoot 'sim_pc\build'
} elseif (-not [IO.Path]::IsPathRooted($BuildDir)) {
    $BuildDir = Join-Path $repoRoot $BuildDir
}

if (-not $SkipBuild) {
    & (Join-Path $PSScriptRoot 'build_sim.ps1') -BuildDir $BuildDir
    if ($LASTEXITCODE -ne 0) { throw 'Simulator build failed' }
}

$simulator = Join-Path $BuildDir 'sim_pc.exe'
if (-not (Test-Path -LiteralPath $simulator -PathType Leaf)) {
    throw "Simulator not found: $simulator. Run tools/build_sim.ps1 first."
}

$python = (Get-Command python -ErrorAction Stop).Source
& $python -c "import PIL" 2>$null
if ($LASTEXITCODE -ne 0) { throw 'Python package Pillow is not installed (needed by raw2png.py)' }

if (-not $OutDir) {
    $OutDir = Join-Path $repoRoot 'docs\screenshots'
} elseif (-not [IO.Path]::IsPathRooted($OutDir)) {
    $OutDir = Join-Path $repoRoot $OutDir
}
if (-not (Test-Path -LiteralPath $OutDir -PathType Container)) {
    New-Item -ItemType Directory -Path $OutDir | Out-Null
}

$telemetryArgs = @()
if ($Telemetry -eq 'demo') { $telemetryArgs += '--demo' }
elseif ($Telemetry -eq 'demo-run') { $telemetryArgs += '--demo-run' }

$runDir = Join-Path ([IO.Path]::GetTempPath()) ('hmi_ui_shots_' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $runDir | Out-Null

$tabs = 'dashboard', 'monitor', 'control', 'graphs', 'diagnostics', 'settings'
$heapSummary = @(
    "6-tab headless shots: telemetry=$Telemetry, $Ms ms virtual time per tab",
    "simulator: $simulator",
    ''
)

try {
    foreach ($tab in $tabs) {
        $raw = Join-Path $runDir "$tab.raw"
        $png = Join-Path $OutDir "$tab.png"
        $shotArgs = @('--shot', $tab, $raw, '--ms', "$Ms") + $telemetryArgs

        Write-Host "--- $tab ---"
        $output = & $simulator @shotArgs 2>&1
        if ($LASTEXITCODE -ne 0) { throw "Headless shot failed: $tab" }
        foreach ($line in $output) { Write-Host "$line" }

        $heapLine = ($output | Where-Object { "$_" -match 'lvgl heap:' } | Select-Object -Last 1)
        if (-not $heapLine) { throw "No LVGL heap report in simulator output: $tab" }
        $heapSummary += "${tab}: $heapLine"

        $rawInfo = Get-Item -LiteralPath $raw -ErrorAction Stop
        $expectedBytes = 800 * 480 * 2
        if ($rawInfo.Length -ne $expectedBytes) {
            throw "Unexpected RGB565 dump size for ${tab}: $($rawInfo.Length), expected $expectedBytes"
        }

        & $python (Join-Path $PSScriptRoot 'raw2png.py') $raw $png
        if ($LASTEXITCODE -ne 0) { throw "PNG conversion failed: $tab" }
    }

    $summaryPath = Join-Path $OutDir 'heap_summary.txt'
    Set-Content -LiteralPath $summaryPath -Value $heapSummary -Encoding ASCII
    Write-Host ''
    Write-Host "PNGs written to: $OutDir"
    Write-Host "Heap summary: $summaryPath"
    Write-Host 'shoot_all: PASS'
} finally {
    if (Test-Path -LiteralPath $runDir) {
        Remove-Item -LiteralPath $runDir -Recurse -Force
    }
}
