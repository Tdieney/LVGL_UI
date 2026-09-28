[CmdletBinding()]
param(
    [switch]$SkipBuild,
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
if (-not (Test-Path -LiteralPath $simulator -PathType Leaf)) {
    throw "Simulator not found: $simulator"
}

function Invoke-SimulatorProcess {
    param(
        [string]$ExePath,
        [string]$Arguments,
        [int]$TimeoutSec = 15
    )
    $pinfo = New-Object System.Diagnostics.ProcessStartInfo
    $pinfo.FileName = $ExePath
    $pinfo.Arguments = $Arguments
    $pinfo.UseShellExecute = $false
    $pinfo.RedirectStandardOutput = $true
    $pinfo.RedirectStandardError = $true
    $process = [System.Diagnostics.Process]::Start($pinfo)
    $stdoutTask = $process.StandardOutput.ReadToEndAsync()
    $stderrTask = $process.StandardError.ReadToEndAsync()
    if (-not $process.WaitForExit($TimeoutSec * 1000)) {
        try { $process.Kill() } catch {}
        throw "Simulator timed out after $TimeoutSec seconds ($Arguments)! Potential lockup detected."
    }
    $stdout = $stdoutTask.Result
    $stderr = $stderrTask.Result
    return [PSCustomObject]@{
        ExitCode = $process.ExitCode
        Stdout   = $stdout
        Stderr   = $stderr
    }
}

function Invoke-SimulatorWithTimeout {
    param(
        [string]$ExePath,
        [string]$Arguments,
        [int]$TimeoutSec = 15
    )
    $res = Invoke-SimulatorProcess -ExePath $ExePath -Arguments $Arguments -TimeoutSec $TimeoutSec
    if ($res.Stdout) { Write-Host $res.Stdout.TrimEnd() }
    if ($res.Stderr) { Write-Error $res.Stderr.TrimEnd() }
    if ($res.ExitCode -ne 0) {
        throw "Simulator failed ($Arguments) with exit code $($res.ExitCode)"
    }
}

$shot = Join-Path ([IO.Path]::GetTempPath()) ('smart_hub_' + [Guid]::NewGuid().ToString('N') + '.raw')
$scenarioDir = Join-Path ([IO.Path]::GetTempPath()) ('smart_hub_shots_' + [Guid]::NewGuid().ToString('N'))
try {
    Write-Host '--- Running Smoke Test ---'
    Invoke-SimulatorWithTimeout -ExePath $simulator -Arguments '--smoke' -TimeoutSec 10

    Write-Host '--- Running Regression Suite ---'
    Invoke-SimulatorWithTimeout -ExePath $simulator -Arguments '--regression' -TimeoutSec 120

    Write-Host '--- Running Single Headless Shot ---'
    Invoke-SimulatorWithTimeout -ExePath $simulator -Arguments "--shot `"$shot`"" -TimeoutSec 10

    $expectedBytes = 800 * 480 * 2
    $actualBytes = (Get-Item -LiteralPath $shot).Length
    if ($actualBytes -ne $expectedBytes) {
        throw "Unexpected framebuffer size: $actualBytes (expected $expectedBytes)"
    }

    Write-Host '--- Running Negative Test: Unwritable Output Destination (NUL) ---'
    $negRes = Invoke-SimulatorProcess -ExePath $simulator -Arguments '--shots NUL' -TimeoutSec 10
    if ($negRes.Stdout) { Write-Host $negRes.Stdout.TrimEnd() }
    if ($negRes.Stderr) { Write-Host $negRes.Stderr.TrimEnd() }

    # Strict assertion (N1): Exit code must be exactly 1; fail if 0 or any other code.
    if ($negRes.ExitCode -eq 0) {
        throw "Scenario generator unexpectedly succeeded with exit code 0 on unwritable destination 'NUL'!"
    }
    if ($negRes.ExitCode -ne 1) {
        throw "Scenario generator returned unexpected exit code $($negRes.ExitCode) on unwritable destination 'NUL' (expected 1)"
    }

    # Strict diagnostic check: Stderr must contain the specific fatal message.
    $expectedFatalPattern = "FATAL: Output directory 'NUL' is not a valid directory or could not be created"
    if ($negRes.Stderr -notmatch [regex]::Escape($expectedFatalPattern)) {
        throw "Scenario generator exit code was 1, but stderr did not match expected diagnostic: '$expectedFatalPattern'. Actual stderr: $($negRes.Stderr)"
    }
    Write-Host "Deterministic negative test passed: exit code 1, diagnostic matched: '$expectedFatalPattern'"

    Write-Host '--- Running Full Scenario Generator (Timeout: 15s) ---'
    New-Item -ItemType Directory -Path $scenarioDir -Force | Out-Null
    Invoke-SimulatorWithTimeout -ExePath $simulator -Arguments "--shots `"$scenarioDir`"" -TimeoutSec 15
    $rawFiles = Get-ChildItem -LiteralPath $scenarioDir -Filter '*.raw'
    $expectedShots = @(
        'splash.raw',
        'home_good.raw',
        'home_moderate.raw',
        'home_poor.raw',
        'home_offline.raw',
        'home_auto_paused_switch.raw',
        'home_sensor_fail.raw',
        'home_relay_pending.raw',
        'home_relay_retry.raw',
        'trends_data.raw',
        'trends_stage1_interim.raw',
        'trends_voc.raw',
        'trends_temp.raw',
        'trends_humid.raw',
        'trends_autonomous_partial.raw',
        'trends_one_sample.raw',
        'trends_empty.raw',
        'devices_grid.raw',
        'devices_settings.raw',
        'home_link_degraded.raw',
        'home_auto_paused.raw',
        'devices_auto_thresholds.raw',
        'devices_settings_manual_only.raw',
        'devices_auto_purifier.raw',
        'devices_auto_humidifier.raw',
        'devices_auto_max_60000.raw',
        'devices_auto_invalid_draft.raw',
        'home_temp_humid_invalid.raw',
        'dropdown_open_top.raw',
        'dropdown_open_bottom.raw'
    )
    $expectedBytesPerShot = 800 * 480 * 2
    if ($rawFiles.Count -ne $expectedShots.Count) {
        throw "Scenario generator emitted $($rawFiles.Count) shots, expected exactly $($expectedShots.Count)"
    }
    foreach ($shotName in $expectedShots) {
        $shotPath = Join-Path $scenarioDir $shotName
        if (-not (Test-Path -LiteralPath $shotPath)) {
            throw "Missing required scenario shot: $shotName"
        }
        $shotSize = (Get-Item -LiteralPath $shotPath).Length
        if ($shotSize -ne $expectedBytesPerShot) {
            throw "Scenario shot $shotName has size $shotSize bytes (expected $expectedBytesPerShot)"
        }
    }
    Write-Host "Generated and verified all $($expectedShots.Count) scenario shots successfully ($expectedBytesPerShot bytes each)."

    Write-Host 'Smart Hub UI regression: PASS'
} finally {
    if (Test-Path -LiteralPath $shot) {
        Remove-Item -LiteralPath $shot -Force
    }
    if (Test-Path -LiteralPath $scenarioDir) {
        Remove-Item -LiteralPath $scenarioDir -Recurse -Force
    }
}
