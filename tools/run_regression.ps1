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
$smoke = Join-Path $BuildDir 'smoke_production.exe'
foreach ($exe in @($simulator, $smoke)) {
    if (-not (Test-Path -LiteralPath $exe -PathType Leaf)) { throw "Not built: $exe" }
}

function Invoke-Process {
    param([string]$ExePath, [string]$Arguments, [int]$TimeoutSec = 15)
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
        throw "Timed out after $TimeoutSec seconds ($ExePath $Arguments)"
    }
    return [PSCustomObject]@{ ExitCode = $process.ExitCode; Stdout = $stdoutTask.Result; Stderr = $stderrTask.Result }
}

function Invoke-Checked {
    param([string]$ExePath, [string]$Arguments, [int]$TimeoutSec = 15)
    $res = Invoke-Process -ExePath $ExePath -Arguments $Arguments -TimeoutSec $TimeoutSec
    if ($res.Stdout) { Write-Host $res.Stdout.TrimEnd() }
    if ($res.Stderr) { Write-Host $res.Stderr.TrimEnd() }
    if ($res.ExitCode -ne 0) { throw "Failed ($ExePath $Arguments) with exit code $($res.ExitCode)" }
}

$frameBytes = 480 * 320 * 2
$shot = Join-Path ([IO.Path]::GetTempPath()) ('smart_studio_' + [Guid]::NewGuid().ToString('N') + '.raw')
$scenarioDir = Join-Path ([IO.Path]::GetTempPath()) ('smart_studio_shots_' + [Guid]::NewGuid().ToString('N'))
try {
    Write-Host '--- Asset generator check ---'
    $python = (Get-Command python -ErrorAction Stop).Source
    Invoke-Checked -ExePath $python -Arguments "`"$(Join-Path $PSScriptRoot 'generate_assets.py')`" --check" -TimeoutSec 30

    Write-Host '--- Production smoke (strict C99, no test hooks) ---'
    Invoke-Checked -ExePath $smoke -Arguments '' -TimeoutSec 10
    $neg = Invoke-Process -ExePath $smoke -Arguments '--fail-check' -TimeoutSec 10
    if ($neg.ExitCode -eq 0) { throw 'Production smoke did not report a deliberate failure' }

    Write-Host '--- Simulator smoke ---'
    Invoke-Checked -ExePath $simulator -Arguments '--smoke' -TimeoutSec 10

    Write-Host '--- Regression suite ---'
    Invoke-Checked -ExePath $simulator -Arguments '--regression' -TimeoutSec 120

    Write-Host '--- Single headless shot ---'
    Invoke-Checked -ExePath $simulator -Arguments "--shot `"$shot`"" -TimeoutSec 10
    $actual = (Get-Item -LiteralPath $shot).Length
    if ($actual -ne $frameBytes) { throw "Unexpected framebuffer size: $actual (expected $frameBytes)" }

    Write-Host '--- Bad arguments and destinations ---'
    $neg = Invoke-Process -ExePath $simulator -Arguments '--shots NUL' -TimeoutSec 10
    if ($neg.ExitCode -ne 1) { throw "--shots NUL returned $($neg.ExitCode), expected 1" }
    $expectedFatal = "FATAL: Output directory 'NUL' is not a valid directory or could not be created"
    if ($neg.Stderr -notmatch [regex]::Escape($expectedFatal)) { throw "Unexpected diagnostic: $($neg.Stderr)" }
    foreach ($bad in @('--shot', '--shots', '--unknown', '--regression extra', '--ppi', '--ppi 39', '--ppi 601', '--ppi abc', '--ppi 96 extra', '--shot NUL')) {
        $res = Invoke-Process -ExePath $simulator -Arguments $bad -TimeoutSec 10
        if ($res.ExitCode -eq 0) { throw "Simulator accepted bad arguments: $bad" }
    }
    Write-Host 'Bad arguments rejected.'

    Write-Host '--- Scenario shots ---'
    New-Item -ItemType Directory -Path $scenarioDir -Force | Out-Null
    Invoke-Checked -ExePath $simulator -Arguments "--shots `"$scenarioDir`"" -TimeoutSec 30
    $expectedShots = @(
        'light_on', 'light_drag', 'light_dim', 'light_off',
        'ac_on', 'ac_min', 'ac_max', 'ac_off',
        'temperature', 'temperature_extreme', 'temperature_unavailable',
        'humidity', 'humidity_unavailable',
        'scenes_home', 'scenes_sleep',
        'connection_idle', 'connection_step', 'connection_pending', 'connection_failed'
    )
    $rawFiles = @(Get-ChildItem -LiteralPath $scenarioDir -Filter '*.raw')
    if ($rawFiles.Count -ne $expectedShots.Count) {
        throw "Scenario generator wrote $($rawFiles.Count) shots, expected $($expectedShots.Count)"
    }
    $hashes = @{}
    foreach ($name in $expectedShots) {
        $path = Join-Path $scenarioDir ($name + '.raw')
        if (-not (Test-Path -LiteralPath $path)) { throw "Missing scenario shot: $name" }
        if ((Get-Item -LiteralPath $path).Length -ne $frameBytes) { throw "Wrong size: $name" }
        $hash = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash
        if ($hashes.ContainsKey($hash)) { throw "Shots $name and $($hashes[$hash]) are identical" }
        $hashes[$hash] = $name
    }
    if ((Get-FileHash -LiteralPath $shot).Hash -ne (Get-FileHash -LiteralPath (Join-Path $scenarioDir 'light_on.raw')).Hash) {
        throw '--shot differs from the light_on scenario'
    }
    Write-Host "Verified $($expectedShots.Count) distinct scenario shots ($frameBytes bytes each)."

    Write-Host 'Smart Studio UI regression: PASS'
} finally {
    if (Test-Path -LiteralPath $shot) { Remove-Item -LiteralPath $shot -Force }
    if (Test-Path -LiteralPath $scenarioDir) { Remove-Item -LiteralPath $scenarioDir -Recurse -Force }
}
