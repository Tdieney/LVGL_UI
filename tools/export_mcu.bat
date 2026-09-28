@echo off
setlocal EnableExtensions
cd /d "%~dp0.." || exit /b 1

set "VERIFY_ONLY="
if /i "%~1"=="--verify" (
    set "VERIFY_ONLY=1"
    set "MCU_DEST="
) else (
    set "MCU_DEST=%~1"
)

for /f "usebackq delims=" %%i in (`powershell -NoProfile -Command "[System.Guid]::NewGuid().ToString('N')"`) do set "STAGE_GUID=%%i"
if "%STAGE_GUID%"=="" set "STAGE_GUID=%RANDOM%_%RANDOM%"
set "STAGE=%TEMP%\smart_hub_ui_export_%STAGE_GUID%"
if exist "%STAGE%" (echo ERROR: temporary directory collision & exit /b 1)
mkdir "%STAGE%\docs" || exit /b 1

for %%f in (lora_comm.h lora_hub_link.h LORA_PROTOCOL.md ui.c ui.h ui_auto.h ui_auto.c ui_internal.h ui_mcu_profile.h ui_types.h ui_theme.h ui_theme.c ui_icons.h ui_icons.c ui_fonts.h ui_fonts.c ui_splash_logo.c) do (
    copy /y "%%f" "%STAGE%\" >nul || (echo ERROR: missing %%f & exit /b 1)
)
for %%f in (HARDWARE_OVERVIEW.md MCU_BASELINE.md README.md UI_DESIGN_BRIEF.md) do (
    copy /y "docs\%%f" "%STAGE%\docs\" >nul || (echo ERROR: missing docs\%%f & exit /b 1)
)

if defined VERIFY_ONLY (
    echo Smart Hub MCU export manifest: PASS
    goto :cleanup_ok
)

set "EXPLICIT_ZIP=%~2"
if not "%EXPLICIT_ZIP%"=="" (
    if exist "%EXPLICIT_ZIP%" (
        echo ERROR: Destination archive already exists: %EXPLICIT_ZIP%
        goto :cleanup_fail
    )
)

set "TMP_ZIP=%STAGE%\package.zip"
powershell -NoProfile -Command "Compress-Archive -Path '%STAGE%\*' -DestinationPath '%TMP_ZIP%'" || goto :cleanup_fail

rem Create publication script inside stage directory
set "PUB_SCRIPT=%STAGE%\publish.ps1"
(
    echo param^(
    echo     [string]$StageZip,
    echo     [string]$ExplicitZip,
    echo     [string]$Cwd,
    echo     [string]$ResultFile
    echo ^)
    echo $ErrorActionPreference = 'Stop'
    echo if ^($ExplicitZip^) {
    echo     $fullExplicit = if ^([System.IO.Path]::IsPathRooted^($ExplicitZip^)^) {
    echo         $ExplicitZip
    echo     } else {
    echo         [System.IO.Path]::GetFullPath^([System.IO.Path]::Combine^($Cwd, $ExplicitZip^)^)
    echo     }
    echo     if ^([System.IO.File]::Exists^($fullExplicit^)^) {
    echo         [Console]::Error.WriteLine^("ERROR: Destination already exists: $fullExplicit"^)
    echo         exit 1
    echo     }
    echo     $parent = [System.IO.Path]::GetDirectoryName^($fullExplicit^)
    echo     if ^($parent -and -not [System.IO.Directory]::Exists^($parent^)^) {
    echo         [System.IO.Directory]::CreateDirectory^($parent^) ^| Out-Null
    echo     }
    echo     try {
    echo         [System.IO.File]::Move^($StageZip, $fullExplicit^)
    echo     } catch {
    echo         [Console]::Error.WriteLine^("ERROR: Failed to move export to explicit destination: $($_.Exception.Message)"^)
    echo         exit 1
    echo     }
    echo     Set-Content -LiteralPath $ResultFile -Value $fullExplicit
    echo     exit 0
    echo } else {
    echo     $ts = $env:TS
    echo     if ^(-not ^($ts -and ^($ts -match '^\d{8}_\d{4}$'^)^)^) {
    echo         $ts = ^(Get-Date -Format 'yyyyMMdd_HHmm'^)
    echo     }
    echo     $baseName = "smart_hub_ui_$ts"
    echo     for ^($c = 0; $c -le 999; $c++^) {
    echo         $candidateName = if ^($c -eq 0^) { "$baseName.zip" } else { "${baseName}_$c.zip" }
    echo         $candidatePath = [System.IO.Path]::Combine^($Cwd, $candidateName^)
    echo         try {
    echo             [System.IO.File]::Move^($StageZip, $candidatePath^)
    echo             Set-Content -LiteralPath $ResultFile -Value $candidatePath
    echo             exit 0
    echo         } catch [System.IO.IOException] {
    echo             continue
    echo         } catch {
    echo             [Console]::Error.WriteLine^("ERROR: Publication failed: $($_.Exception.Message)"^)
    echo             exit 1
    echo         }
    echo     }
    echo     [Console]::Error.WriteLine^("ERROR: Exhausted available unique filenames for export (0..999)"^)
    echo     exit 1
    echo }
) > "%PUB_SCRIPT%" || goto :cleanup_fail

powershell -NoProfile -ExecutionPolicy Bypass -File "%PUB_SCRIPT%" -StageZip "%TMP_ZIP%" -ExplicitZip "%EXPLICIT_ZIP%" -Cwd "%CD%" -ResultFile "%STAGE%\out_zip.txt" || goto :cleanup_fail

if not exist "%STAGE%\out_zip.txt" goto :cleanup_fail
set /p OUT_ZIP=<"%STAGE%\out_zip.txt"
echo Created: %OUT_ZIP%

if not "%MCU_DEST%"=="" (
    set "DEPLOY_SCRIPT=%STAGE%\deploy.ps1"
    (
        echo param^(
        echo     [string]$OutZip,
        echo     [string]$McuDest
        echo ^)
        echo $ErrorActionPreference = 'Stop'
        echo if ^(-not [System.IO.Directory]::Exists^($McuDest^)^) {
        echo     [System.IO.Directory]::CreateDirectory^($McuDest^) ^| Out-Null
        echo }
        echo $fullDest = [System.IO.Path]::GetFullPath^($McuDest^)
        echo $fullOut = [System.IO.Path]::GetFullPath^($OutZip^)
        echo $targetZip = [System.IO.Path]::Combine^($fullDest, [System.IO.Path]::GetFileName^($fullOut^)^)
        echo if ^($fullOut -ine $targetZip^) {
        echo     if ^([System.IO.File]::Exists^($targetZip^)^) {
        echo         [Console]::Error.WriteLine^("ERROR: Destination archive already exists in MCU destination: $targetZip"^)
        echo         exit 1
        echo     }
        echo     [System.IO.File]::Copy^($fullOut, $targetZip, $false^)
        echo }
        echo $extractDir = [System.IO.Path]::Combine^($fullDest, 'smart_hub_ui'^)
        echo Expand-Archive -LiteralPath $fullOut -DestinationPath $extractDir -Force
        echo Write-Host "Extracted to: $extractDir\"
    ) > "%STAGE%\deploy.ps1" || goto :cleanup_fail

    powershell -NoProfile -ExecutionPolicy Bypass -File "%STAGE%\deploy.ps1" -OutZip "%OUT_ZIP%" -McuDest "%MCU_DEST%" || goto :cleanup_fail
)

:cleanup_ok
if exist "%STAGE%" rmdir /s /q "%STAGE%"
endlocal
exit /b 0

:cleanup_fail
if exist "%STAGE%" rmdir /s /q "%STAGE%"
endlocal
exit /b 1
