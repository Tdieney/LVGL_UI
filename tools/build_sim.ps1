[CmdletBinding()]
param(
    [ValidateSet('Release', 'Debug')]
    [string]$Configuration = 'Release',
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

if (-not $Compiler) {
    $bundled = 'C:\Toolchains\w64devkit\bin\gcc.exe'
    if (Test-Path -LiteralPath $bundled -PathType Leaf) {
        $Compiler = $bundled
    } else {
        $Compiler = (Get-Command gcc -ErrorAction Stop).Source
    }
}
$Compiler = [IO.Path]::GetFullPath($Compiler)
if (-not (Test-Path -LiteralPath $Compiler -PathType Leaf)) {
    throw "C compiler not found: $Compiler"
}

$cmake = (Get-Command cmake -ErrorAction Stop).Source
$compilerDir = Split-Path -Parent $Compiler
if (($env:PATH -split ';') -notcontains $compilerDir) {
    $env:PATH = $compilerDir + ';' + $env:PATH
}

$configureArgs = @(
    '-S', (Join-Path $repoRoot 'sim_pc'),
    '-B', $BuildDir,
    '-G', 'Ninja',
    "-DCMAKE_BUILD_TYPE=$Configuration",
    "-DCMAKE_C_COMPILER=$($Compiler.Replace('\', '/'))"
)

# Once the offline LVGL cache is populated, configure must stay deterministic
# and must NOT re-fetch from GitHub (full regression used to hang here).
if (Test-Path -LiteralPath (Join-Path $BuildDir '_deps\lvgl-src')) {
    $configureArgs += '-DFETCHCONTENT_FULLY_DISCONNECTED=ON'
}

Write-Host "Configuring simulator in $BuildDir"
& $cmake @configureArgs
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed ($LASTEXITCODE)" }

Write-Host "Building simulator ($Configuration)"
# Build only the sim_pc target: lvgl_examples/lvgl_demos are not used by the
# simulator and compiling them doubles the build time.
& $cmake --build $BuildDir --target sim_pc --config $Configuration
if ($LASTEXITCODE -ne 0) { throw "CMake build failed ($LASTEXITCODE)" }

$simulator = Join-Path $BuildDir 'sim_pc.exe'
if (-not (Test-Path -LiteralPath $simulator -PathType Leaf)) {
    throw "Build completed without simulator: $simulator"
}
Write-Host "Simulator ready: $simulator"
