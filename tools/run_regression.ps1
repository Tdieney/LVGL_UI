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

function Invoke-SimulatorWithTimeout {
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
    if ($stdout) { Write-Host $stdout.TrimEnd() }
    if ($stderr) { Write-Error $stderr.TrimEnd() }
    if ($process.ExitCode -ne 0) {
        throw "Simulator failed ($Arguments) with exit code $($process.ExitCode)"
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

    Write-Host '--- Running Full Scenario Generator (Timeout: 15s) ---'
    New-Item -ItemType Directory -Path $scenarioDir -Force | Out-Null
    Invoke-SimulatorWithTimeout -ExePath $simulator -Arguments "--shots `"$scenarioDir`"" -TimeoutSec 15
    $rawFiles = Get-ChildItem -LiteralPath $scenarioDir -Filter '*.raw'
    if ($rawFiles.Count -lt 10) {
        throw "Scenario generator emitted only $($rawFiles.Count) shots, expected at least 10"
    }
    Write-Host "Generated $($rawFiles.Count) scenario shots successfully."

    Write-Host 'Smart Hub UI regression: PASS'
} finally {
    if (Test-Path -LiteralPath $shot) {
        Remove-Item -LiteralPath $shot -Force
    }
    if (Test-Path -LiteralPath $scenarioDir) {
        Remove-Item -LiteralPath $scenarioDir -Recurse -Force
    }
}
