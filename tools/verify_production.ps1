[CmdletBinding()]
param(
    [string]$BuildDir = '',
    [string]$Compiler = ''
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$verificationPassed = $false
$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if (-not $BuildDir) {
    $BuildDir = Join-Path $repoRoot 'sim_pc\build'
} elseif (-not [IO.Path]::IsPathRooted($BuildDir)) {
    $BuildDir = Join-Path $repoRoot $BuildDir
}

function Invoke-ProcessWithTimeout {
    param(
        [Parameter(Mandatory=$true)][string]$FilePath,
        [string[]]$ArgumentList = @(),
        [string]$RawArguments = '',
        [string]$WorkingDirectory = '',
        [int]$TimeoutSeconds = 60,
        [string]$PhaseName = 'Process'
    )
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $FilePath
    if ($RawArguments) {
        $psi.Arguments = $RawArguments
    } else {
        $escapedArgs = $ArgumentList | ForEach-Object {
            if ($_ -match '[\s"]') {
                "`"" + ($_ -replace '"', '\"') + "`""
            } else {
                $_
            }
        }
        $psi.Arguments = ($escapedArgs -join ' ')
    }
    if ($WorkingDirectory) { $psi.WorkingDirectory = $WorkingDirectory }
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $psi.UseShellExecute = $false
    $psi.CreateNoWindow = $true

    $proc = [System.Diagnostics.Process]::Start($psi)
    if ($null -eq $proc) {
        throw "Failed to start process: $FilePath ($PhaseName)"
    }

    $stdoutTask = $proc.StandardOutput.ReadToEndAsync()
    $stderrTask = $proc.StandardError.ReadToEndAsync()

    $completed = $proc.WaitForExit($TimeoutSeconds * 1000)
    if (-not $completed) {
        try {
            & taskkill.exe /PID $proc.Id /T /F | Out-Null
        } catch {}
        throw "Watchdog timeout ($TimeoutSeconds s) exceeded during $PhaseName"
    }

    return [PSCustomObject]@{
        ExitCode = $proc.ExitCode
        Stdout   = $stdoutTask.Result
        Stderr   = $stderrTask.Result
    }
}

Write-Host "========================================================"
Write-Host " Phase 1: Verifying MCU Export Manifest & Exporting Bundle"
Write-Host "========================================================"
$exportScript = Join-Path $repoRoot 'tools\export_mcu.bat'

# Verify manifest check
$manifestRes = Invoke-ProcessWithTimeout -FilePath 'cmd.exe' -RawArguments "/c call `"$exportScript`" --verify" -TimeoutSeconds 30 -PhaseName 'Manifest Verification'
if ($manifestRes.ExitCode -ne 0) {
    if ($manifestRes.Stdout) { Write-Host $manifestRes.Stdout }
    if ($manifestRes.Stderr) { Write-Error $manifestRes.Stderr }
    throw "MCU export manifest verification failed with exit code $($manifestRes.ExitCode)"
}
Write-Host "MCU export manifest: PASS"

# -------------------------------------------------------------
# W4 Collision Safety & Ownership Unit Tests (Disposable Fixture)
# -------------------------------------------------------------
Write-Host "`nTesting W4 export collision safety and artifact ownership..."
$fixtureGuid = [System.Guid]::NewGuid().ToString("N")
$fixtureDir = Join-Path $env:TEMP "smart_hub_ui_w4_fixture_$fixtureGuid"
New-Item -ItemType Directory -Path $fixtureDir -Force | Out-Null
try {
    # 1. Sentinel preservation: create dummy existing export zip
    $sentinelZip = Join-Path $fixtureDir 'smart_hub_ui_sentinel.zip'
    [IO.File]::WriteAllBytes($sentinelZip, [byte[]]@(0x50, 0x4B, 0x05, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00))
    $sentinelHashBefore = (Get-FileHash -LiteralPath $sentinelZip -Algorithm SHA256).Hash

    # Export into fixture with custom zip path (separate from sentinel)
    $customZip = Join-Path $fixtureDir 'custom_export.zip'
    $expRes = Invoke-ProcessWithTimeout -FilePath 'cmd.exe' -RawArguments "/c call `"$exportScript`" `"$fixtureDir\dest`" `"$customZip`"" -TimeoutSeconds 60 -PhaseName 'W4 Custom Zip Export'
    if ($expRes.ExitCode -ne 0) { throw "Custom zip export failed with code $($expRes.ExitCode)" }
    if (-not (Test-Path -LiteralPath $customZip -PathType Leaf)) { throw "Expected custom zip not created: $customZip" }

    # Verify sentinel remains byte-identical
    $sentinelHashAfter = (Get-FileHash -LiteralPath $sentinelZip -Algorithm SHA256).Hash
    if ($sentinelHashBefore -ne $sentinelHashAfter) {
        throw "Sentinel ZIP was modified or overwritten by export!"
    }

    # 2. Same-minute collision isolation: two exports with custom zip paths
    $customZip2 = Join-Path $fixtureDir 'custom_export2.zip'
    $expRes2 = Invoke-ProcessWithTimeout -FilePath 'cmd.exe' -RawArguments "/c call `"$exportScript`" `"$fixtureDir\dest2`" `"$customZip2`"" -TimeoutSeconds 60 -PhaseName 'W4 Concurrent Export'
    if ($expRes2.ExitCode -ne 0) { throw "Second zip export failed with code $($expRes2.ExitCode)" }
    if (-not (Test-Path -LiteralPath $customZip2 -PathType Leaf)) { throw "Expected second zip not created: $customZip2" }
    if ((Get-Item -LiteralPath $customZip).Length -eq 0 -or (Get-Item -LiteralPath $customZip2).Length -eq 0) {
        throw "Exported archives are zero-length"
    }

    Write-Host "W4 collision safety: PASS (sentinel preserved, unique artifacts owned)"
} finally {
    Remove-Item -LiteralPath $fixtureDir -Recurse -Force -ErrorAction SilentlyContinue
}

# Locate cached offline LVGL source tree
$lvglSrc = Join-Path $BuildDir '_deps\lvgl-src'
if (-not (Test-Path -LiteralPath (Join-Path $lvglSrc 'lvgl.h') -PathType Leaf)) {
    throw "Cached offline LVGL source tree not found at $lvglSrc. Please run tools/build_sim.bat first."
}

# Locate C compiler and CMake
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
$compilerDir = Split-Path -Parent $Compiler
if (($env:PATH -split ';') -notcontains $compilerDir) {
    $env:PATH = $compilerDir + ';' + $env:PATH
}
$cmake = (Get-Command cmake -ErrorAction Stop).Source

# Create validated temporary staging directory
$guid = [System.Guid]::NewGuid().ToString("N")
$stageDir = Join-Path $env:TEMP "smart_hub_ui_isolated_verify_$guid"
$stageExportDest = Join-Path $stageDir 'export_dest'
New-Item -ItemType Directory -Path $stageExportDest -Force | Out-Null

$createdZipPath = $null

try {
    # Define exact unique task-owned zip path inside the stage directory
    $taskZipPath = Join-Path $stageDir "smart_hub_ui_task_$guid.zip"
    $createdZipPath = $taskZipPath

    # Export MCU package directly into staging export destination with exact task-owned zip
    Write-Host "`nExporting MCU bundle to staging directory..."
    $exportRes = Invoke-ProcessWithTimeout -FilePath 'cmd.exe' -RawArguments "/c call `"$exportScript`" `"$stageExportDest`" `"$taskZipPath`"" -TimeoutSeconds 60 -PhaseName 'MCU Export'
    if ($exportRes.ExitCode -ne 0) {
        if ($exportRes.Stdout) { Write-Host $exportRes.Stdout }
        if ($exportRes.Stderr) { Write-Error $exportRes.Stderr }
        throw "MCU export execution failed with exit code $($exportRes.ExitCode)"
    }

    if (-not (Test-Path -LiteralPath $taskZipPath -PathType Leaf)) {
        throw "Task-owned export archive not found at $taskZipPath"
    }

    $stageSrcDir = Join-Path $stageExportDest 'smart_hub_ui'
    if (-not (Test-Path -LiteralPath $stageSrcDir -PathType Container)) {
        throw "Exported smart_hub_ui directory not found at $stageSrcDir"
    }

    Write-Host "`n========================================================"
    Write-Host " Phase 2: Isolated Host Build from Exported Artifacts"
    Write-Host "========================================================"
    Write-Host "  Exported Staging: $stageSrcDir"
    Write-Host "  LVGL Source:      $lvglSrc"
    Write-Host "  Compiler:         $Compiler"

    # Verify key exported files exist in the artifact directory
    $expectedArtifactFiles = @(
        'ui.c', 'ui.h', 'ui_auto.c', 'ui_auto.h',
        'ui_theme.c', 'ui_theme.h', 'ui_icons.c', 'ui_icons.h',
        'ui_fonts.c', 'ui_fonts.h', 'ui_splash_logo.c',
        'ui_internal.h', 'ui_mcu_profile.h', 'ui_types.h',
        'lora_comm.h', 'lora_hub_link.h'
    )
    foreach ($f in $expectedArtifactFiles) {
        $p = Join-Path $stageSrcDir $f
        if (-not (Test-Path -LiteralPath $p -PathType Leaf)) {
            throw "Exported artifact verification failed: missing expected file $f in $stageSrcDir"
        }
    }
    Write-Host "Export artifact membership: PASS ($($expectedArtifactFiles.Count) verified files)"

    # Copy production smoke runner into the staged source directory
    $smokeSrc = Join-Path $repoRoot 'sim_pc\smoke_production.c'
    if (-not (Test-Path -LiteralPath $smokeSrc -PathType Leaf)) {
        throw "smoke_production.c not found at $smokeSrc"
    }
    Copy-Item -LiteralPath $smokeSrc -Destination (Join-Path $stageSrcDir 'smoke_production.c') -Force

    # Write isolated lv_conf.h
    $lvConfContent = @"
#ifndef LV_CONF_H
#define LV_CONF_H

#include "ui_mcu_profile.h"

#define LV_COLOR_DEPTH 16
#define LV_COLOR_16_SWAP 0
#define LV_MEM_CUSTOM 0
#define LV_MEM_SIZE UI_LVGL_HEAP_BYTES

/* Study 12: all surfaces are flat fills; gradient cache disabled */
#define LV_GRAD_CACHE_DEF_SIZE 0

#define LV_DISP_DEF_REFR_PERIOD 30
#define LV_INDEV_DEF_READ_PERIOD 30
#define LV_TICK_CUSTOM 0
#define LV_DPI_DEF 130
#define LV_USE_LOG 1
#define LV_LOG_LEVEL LV_LOG_LEVEL_WARN
#define LV_LOG_PRINTF 1

/* Production fonts */
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 0
#define LV_FONT_MONTSERRAT_18 0
#define LV_FONT_MONTSERRAT_20 0
#define LV_FONT_MONTSERRAT_24 0
#define LV_FONT_MONTSERRAT_30 0
#define LV_FONT_MONTSERRAT_34 0
#define LV_FONT_MONTSERRAT_48 0
#define LV_FONT_DEFAULT &lv_font_montserrat_14

#endif /* LV_CONF_H */
"@
    Set-Content -Path (Join-Path $stageSrcDir 'lv_conf.h') -Value $lvConfContent -Encoding Ascii

    # Write isolated CMakeLists.txt
    $lvglSrcCmake = $lvglSrc.Replace('\', '/')
    $cmakeContent = @"
cmake_minimum_required(VERSION 3.20)
project(smart_hub_ui_isolated_smoke C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(LV_CONF_PATH "`${CMAKE_CURRENT_SOURCE_DIR}/lv_conf.h" CACHE FILEPATH "" FORCE)
add_subdirectory("$lvglSrcCmake" lvgl EXCLUDE_FROM_ALL)

add_executable(smoke_production
    smoke_production.c
    ui.c
    ui_auto.c
    ui_theme.c
    ui_icons.c
    ui_fonts.c
    ui_splash_logo.c
)

target_include_directories(smoke_production PRIVATE
    `${CMAKE_CURRENT_SOURCE_DIR}
    "$lvglSrcCmake"
)

target_compile_definitions(smoke_production PRIVATE
    NDEBUG
    LV_CONF_INCLUDE_SIMPLE
)

target_link_libraries(smoke_production PRIVATE lvgl)

if(MSVC)
    target_compile_options(smoke_production PRIVATE /W4 /WX /O2)
else()
    target_compile_options(smoke_production PRIVATE -Wall -Wextra -Werror -O2)
endif()
"@
    Set-Content -Path (Join-Path $stageSrcDir 'CMakeLists.txt') -Value $cmakeContent -Encoding Ascii

    # Configure isolated CMake with watchdog
    $stageBuildDir = Join-Path $stageSrcDir 'build'
    Write-Host "`nConfiguring isolated build with CMake (watchdog: 60s)..."
    $configArgs = @(
        '-S', $stageSrcDir,
        '-B', $stageBuildDir,
        '-G', 'Ninja',
        '-DCMAKE_BUILD_TYPE=Release',
        "-DCMAKE_C_COMPILER=$($Compiler.Replace('\', '/'))"
    )
    $cfgRes = Invoke-ProcessWithTimeout -FilePath $cmake -ArgumentList $configArgs -TimeoutSeconds 60 -PhaseName 'CMake Configure'
    if ($cfgRes.ExitCode -ne 0) {
        if ($cfgRes.Stdout) { Write-Host $cfgRes.Stdout }
        if ($cfgRes.Stderr) { Write-Error $cfgRes.Stderr }
        throw "Isolated CMake configuration failed with exit code $($cfgRes.ExitCode)"
    }

    # Build isolated smoke executable with watchdog
    Write-Host "`nCompiling isolated smoke_production (Release, -Wall -Wextra -Werror, watchdog: 60s)..."
    $buildArgs = @('--build', $stageBuildDir, '--target', 'smoke_production', '--config', 'Release')
    $bldRes = Invoke-ProcessWithTimeout -FilePath $cmake -ArgumentList $buildArgs -TimeoutSeconds 60 -PhaseName 'CMake Build'
    if ($bldRes.ExitCode -ne 0) {
        if ($bldRes.Stdout) { Write-Host $bldRes.Stdout }
        if ($bldRes.Stderr) { Write-Error $bldRes.Stderr }
        throw "Isolated CMake build failed with exit code $($bldRes.ExitCode)"
    }

    $smokeExe = Join-Path $stageBuildDir 'smoke_production.exe'
    if (-not (Test-Path -LiteralPath $smokeExe -PathType Leaf)) {
        throw "Isolated smoke_production.exe not found at $smokeExe"
    }

    Write-Host "`n========================================================"
    Write-Host " Phase 3: Running Production Smoke Test (Clean Run, watchdog: 30s)"
    Write-Host "========================================================"
    $cleanRes = Invoke-ProcessWithTimeout -FilePath $smokeExe -ArgumentList @() -TimeoutSeconds 30 -PhaseName 'Clean Smoke Run'
    if ($cleanRes.ExitCode -ne 0) {
        if ($cleanRes.Stdout) { Write-Host $cleanRes.Stdout }
        if ($cleanRes.Stderr) { Write-Error $cleanRes.Stderr }
        throw "Isolated smoke_production clean run failed with exit code $($cleanRes.ExitCode) (expected 0)"
    }
    if ($cleanRes.Stdout) { Write-Host $cleanRes.Stdout }
    Write-Host "Clean run: PASS (exit code 0)"

    Write-Host "`n========================================================"
    Write-Host " Phase 4: Running Negative Control (--fail-check, watchdog: 30s)"
    Write-Host "========================================================"
    # Must launch successfully, exit with strictly 1, and output expected error to stderr.
    # No catch-all: launch failures, crashes, or timeouts throw and fail verification.
    $failRes = Invoke-ProcessWithTimeout -FilePath $smokeExe -ArgumentList @('--fail-check') -TimeoutSeconds 30 -PhaseName 'Negative Control'
    Write-Host "Process exit code: $($failRes.ExitCode)"
    if ($failRes.Stdout) { Write-Host "stdout:`n$($failRes.Stdout)" }
    if ($failRes.Stderr) { Write-Host "stderr:`n$($failRes.Stderr)" }

    if ($failRes.ExitCode -ne 1) {
        throw "Negative control check failed: expected exit code 1, but received $($failRes.ExitCode)"
    }
    if ($failRes.Stderr -notmatch '\[SMOKE ERROR\] Check failed: 1 == 2') {
        throw "Negative control check failed: stderr did not contain expected '[SMOKE ERROR] Check failed: 1 == 2'"
    }
    Write-Host "Negative control: PASS (strictly exit code 1 with verified error message)"

    Write-Host "`n========================================================"
    Write-Host " Production verification & Release checks: ALL PASSED "
    Write-Host "========================================================"
    $verificationPassed = $true
}
finally {
    # Guarded cleanup of staging directory: must be strictly rooted inside TEMP and not repo root
    $canonicalStage = [IO.Path]::GetFullPath($stageDir)
    $canonicalTemp = [IO.Path]::GetFullPath($env:TEMP).TrimEnd([IO.Path]::DirectorySeparatorChar, [IO.Path]::AltDirectorySeparatorChar)
    $tempPrefix = $canonicalTemp + [IO.Path]::DirectorySeparatorChar

    if ($canonicalStage.StartsWith($tempPrefix, [System.StringComparison]::OrdinalIgnoreCase) -and
        $canonicalStage -ne $canonicalTemp -and
        $canonicalStage -ne $repoRoot -and
        (Test-Path -LiteralPath $canonicalStage)) {
        if ($verificationPassed) {
            Write-Host "`nCleaning up isolated staging directory: $canonicalStage"
            Remove-Item -LiteralPath $canonicalStage -Recurse -Force -ErrorAction SilentlyContinue
        } else {
            Write-Host "`nVerification failed: retaining isolated staging directory for diagnostic evidence: $canonicalStage"
        }
    }
}

