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

$shot = Join-Path ([IO.Path]::GetTempPath()) ('smart_hub_' + [Guid]::NewGuid().ToString('N') + '.raw')
try {
    & $simulator --smoke
    if ($LASTEXITCODE -ne 0) { throw 'Smart Hub smoke test failed' }

    & $simulator --shot $shot
    if ($LASTEXITCODE -ne 0) { throw 'Smart Hub headless shot failed' }

    $expectedBytes = 800 * 480 * 2
    $actualBytes = (Get-Item -LiteralPath $shot).Length
    if ($actualBytes -ne $expectedBytes) {
        throw "Unexpected framebuffer size: $actualBytes (expected $expectedBytes)"
    }
    Write-Host 'Smart Hub UI regression: PASS'
} finally {
    if (Test-Path -LiteralPath $shot) {
        Remove-Item -LiteralPath $shot -Force
    }
}
