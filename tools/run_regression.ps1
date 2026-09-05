[CmdletBinding()]
param(
    [switch]$SkipBuild,
    [switch]$SkipShots,
    [ValidateRange(1, 600000)]
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

    $controlModes = @(
        @{ Name = 'speed';    Match = '"Iq Limit"';   RingHidden = $true },
        @{ Name = 'torque';   Match = '"Speed Limit"';  RingHidden = $true },
        @{ Name = 'position'; Match = 'arc\s+vis';    RingHidden = $false }
    )
    foreach ($mode in $controlModes) {
        $layoutOutput = & $simulator --layout control --mode $mode.Name --ms 4000 --demo 2>&1
        if ($LASTEXITCODE -ne 0) { throw "Control layout dump failed: $($mode.Name)" }
        $layoutText = $layoutOutput -join "`n"
        if ($layoutText -notmatch $mode.Match) {
            throw "Control layout did not show $($mode.Name) content (missing '$($mode.Match)')"
        }
        if ($layoutText -match '\sHID\s') {
            throw "Control $($mode.Name): inactive mode subtree must not remain hidden/resident"
        }
        if ($mode.RingHidden -and $layoutText -match 'arc\s+vis') {
            throw "Control $($mode.Name): position ring must not be resident"
        }
    }
    Write-Host 'Control headless modes: PASS'

    & $simulator --test-nav
    if ($LASTEXITCODE -ne 0) { throw 'Navigation pointer hit-map test failed' }

    & $simulator --test-dashboard-bands
    if ($LASTEXITCODE -ne 0) { throw 'Dashboard speed-band test failed' }

    # xSPI redraw guardrails. POSITION keeps the handle at the input sampling
    # rate but must not invalidate its static 212 px ring or redraw the large
    # number on every pointer sample. Graphs deliberately retain a 2 Hz
    # scrolling trace per series; the bound catches accidental multi-chart
    # refreshes in one lane without changing that reviewed behavior.
    foreach ($levelMode in 'speed', 'torque') {
        $levelProfile = & $simulator --profile control --mode $levelMode --ms 10000 --demo-run 2>&1
        if ($LASTEXITCODE -ne 0) { throw "Control $levelMode flush profile failed" }
        $levelText = $levelProfile -join "`n"
        if ($levelText -notmatch 'px/s=(\d+).*max-frame=(\d+)') {
            throw "Control $levelMode flush profile did not report counters"
        }
        if ([uint64]$Matches[1] -gt [uint64]10000 -or [uint64]$Matches[2] -gt [uint64]16000) {
            throw "Control $levelMode redraw budget exceeded: $levelText"
        }
        Write-Host $levelText
    }

    $positionProfile = & $simulator --profile control --mode position --ms 10000 --demo-run --drag 2>&1
    if ($LASTEXITCODE -ne 0) { throw 'Control POSITION flush profile failed' }
    $positionText = $positionProfile -join "`n"
    if ($positionText -notmatch 'px/s=(\d+).*max-frame=(\d+)') {
        throw 'Control POSITION flush profile did not report counters'
    }
    if ([uint64]$Matches[1] -gt [uint64]120000 -or [uint64]$Matches[2] -gt [uint64]15000) {
        throw "Control POSITION redraw budget exceeded: $positionText"
    }
    Write-Host $positionText

    $graphsProfile = & $simulator --profile graphs --ms 10000 --demo-run 2>&1
    if ($LASTEXITCODE -ne 0) { throw 'Graphs flush profile failed' }
    $graphsText = $graphsProfile -join "`n"
    if ($graphsText -notmatch 'px/s=(\d+).*max-frame=(\d+)') {
        throw 'Graphs flush profile did not report counters'
    }
    if ([uint64]$Matches[1] -gt [uint64]360000 -or [uint64]$Matches[2] -gt [uint64]50000) {
        throw "Graphs redraw budget exceeded: $graphsText"
    }
    Write-Host $graphsText

    # Tab switches must stay scoped to the persistent 712x414 content host and
    # render in exactly two batches: responsive chrome first, content second.
    # The old lv_obj_clean(ui_MainScreen) path measured exactly 384,000 px / 48
    # flush calls for every pair (a full 800x480 resend). Check the complete
    # six-tab cycle so a screen-specific builder cannot reintroduce it.
    $switchPairs = @(
        @('dashboard', 'monitor'),
        @('monitor', 'control'),
        @('control', 'graphs'),
        @('graphs', 'diagnostics'),
        @('diagnostics', 'settings'),
        @('settings', 'dashboard')
    )
    $switchMax = [uint64]0
    foreach ($pair in $switchPairs) {
        $switchProfile = & $simulator --profile-switch $pair[0] $pair[1] --demo 2>&1
        if ($LASTEXITCODE -ne 0) { throw "Tab-switch flush profile failed: $($pair[0]) -> $($pair[1])" }
        $switchText = $switchProfile -join "`n"
        if ($switchText -notmatch 'pixels=(\d+).*frames=(\d+).*chrome-frame=(\d+).*max-frame=(\d+).*calls=(\d+)') {
            throw "Tab-switch flush profile did not report counters: $switchText"
        }
        $switchPixels = [uint64]$Matches[1]
        $switchFrames = [uint32]$Matches[2]
        $chromePixels = [uint64]$Matches[3]
        $switchBurst = [uint64]$Matches[4]
        if ($switchPixels -gt $switchMax) { $switchMax = $switchPixels }
        if ($switchPixels -gt [uint64]340000) {
            throw "Tab-switch redraw budget exceeded: $switchText"
        }
        if ($switchFrames -ne 2 -or $chromePixels -gt [uint64]22000 -or $switchBurst -gt [uint64]315000) {
            throw "Tab-switch two-phase budget exceeded: $switchText"
        }
        Write-Host $switchText
    }
    Write-Host "xSPI redraw profiles: PASS (2-phase chrome/content; switch max $switchMax px; full-screen baseline 384000 px)"

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
