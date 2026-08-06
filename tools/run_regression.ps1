[CmdletBinding()]
param(
    [switch]$SkipBuild,
    [switch]$SkipShots,
    [int]$StressMs = 120000,
    [string]$BuildDir = '',
    [string]$Compiler = ''
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if (-not $BuildDir) {
    $BuildDir = Join-Path $repoRoot 'sim_pc\build'
} elseif (-not [IO.Path]::IsPathRooted($BuildDir)) {
    $BuildDir = Join-Path $repoRoot $BuildDir
}

if (-not $SkipBuild) {
    & (Join-Path $PSScriptRoot 'build_sim.ps1') -BuildDir $BuildDir -Compiler $Compiler
    if ($LASTEXITCODE -ne 0) { throw 'Simulator build failed' }
}

$simulator = Join-Path $BuildDir 'sim_pc.exe'
$lvglSource = Join-Path $BuildDir '_deps\lvgl-src'
if (-not (Test-Path -LiteralPath $simulator -PathType Leaf)) {
    throw "Simulator not found: $simulator. Run tools/build_sim.ps1 first."
}
if (-not (Test-Path -LiteralPath $lvglSource -PathType Container)) {
    throw "Cached LVGL source not found: $lvglSource. Run tools/build_sim.ps1 first."
}

if (-not $Compiler) {
    $bundled = 'C:\Toolchains\w64devkit\bin\gcc.exe'
    if (Test-Path -LiteralPath $bundled -PathType Leaf) {
        $Compiler = $bundled
    } else {
        $Compiler = (Get-Command gcc -ErrorAction Stop).Source
    }
}
$Compiler = [IO.Path]::GetFullPath($Compiler)
$compilerDir = Split-Path -Parent $Compiler
if (($env:PATH -split ';') -notcontains $compilerDir) {
    $env:PATH = $compilerDir + ';' + $env:PATH
}

$runDir = Join-Path ([IO.Path]::GetTempPath()) ('hmi_ui_regression_' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $runDir | Out-Null

try {
    $includeArgs = @(
        "-I$(Join-Path $repoRoot 'sim_pc')",
        "-I$repoRoot",
        '-isystem', $lvglSource
    )
    $uiSources = @('screens.c', 'ui.c', 'actions.c', 'demo_sim.c')

    foreach ($demoMode in 0, 1) {
        foreach ($source in $uiSources) {
            $syntaxArgs = @(
                '-std=c11', '-Wall', '-Wextra', '-Werror',
                '-DLV_CONF_INCLUDE_SIMPLE', '-DLV_LVGL_H_INCLUDE_SIMPLE',
                "-DUI_DEMO_SIM=$demoMode"
            ) + $includeArgs + @('-fsyntax-only', (Join-Path $repoRoot $source))
            & $Compiler @syntaxArgs
            if ($LASTEXITCODE -ne 0) { throw "Syntax check failed: UI_DEMO_SIM=$demoMode $source" }
        }
        Write-Host "Syntax UI_DEMO_SIM=$demoMode`: PASS"
    }

    $positionExe = Join-Path $runDir 'test_pos_angle.exe'
    & $Compiler -std=c11 -Wall -Wextra -Werror "-I$repoRoot" `
        (Join-Path $repoRoot 'tests\test_pos_angle.c') -o $positionExe -lm
    if ($LASTEXITCODE -ne 0) { throw 'Position test compilation failed' }
    & $positionExe
    if ($LASTEXITCODE -ne 0) { throw 'Position test failed' }

    & $simulator --stress-demo --ms $StressMs
    if ($LASTEXITCODE -ne 0) { throw 'Demo/Monitor stress failed' }

    if (-not $SkipShots) {
        foreach ($tab in 'dashboard', 'monitor', 'control', 'graphs', 'diagnostics', 'settings') {
            $shot = Join-Path $runDir "$tab.raw"
            & $simulator --shot $tab $shot --ms 10000 --demo
            if ($LASTEXITCODE -ne 0) { throw "Headless shot failed: $tab" }
        }
    }

    Write-Host 'UI regression: PASS'
} finally {
    if (Test-Path -LiteralPath $runDir) {
        Remove-Item -LiteralPath $runDir -Recurse -Force
    }
}
