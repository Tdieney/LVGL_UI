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

set "STAGE=%TEMP%\smart_hub_ui_export_%RANDOM%_%RANDOM%"
if exist "%STAGE%" (echo ERROR: temporary directory collision & exit /b 1)
mkdir "%STAGE%\docs" || exit /b 1

for %%f in (ui.c ui.h ui_mcu_profile.h) do (
    copy /y "%%f" "%STAGE%\" >nul || (echo ERROR: missing %%f & exit /b 1)
)
for %%f in (HARDWARE_OVERVIEW.md MCU_BASELINE.md) do (
    copy /y "docs\%%f" "%STAGE%\docs\" >nul || (echo ERROR: missing docs\%%f & exit /b 1)
)

if defined VERIFY_ONLY (
    echo Smart Hub MCU export manifest: PASS
    goto :cleanup
)

for /f %%i in ('powershell -NoProfile -Command "Get-Date -Format yyyyMMdd_HHmm"') do set "TS=%%i"
set "ZIP=smart_hub_ui_%TS%.zip"
powershell -NoProfile -Command "Compress-Archive -Path '%STAGE%\*' -DestinationPath '%CD%\%ZIP%' -Force" || exit /b 1
echo Created: %CD%\%ZIP%

if not "%MCU_DEST%"=="" (
    if not exist "%MCU_DEST%" (echo ERROR: destination does not exist & exit /b 1)
    copy /y "%CD%\%ZIP%" "%MCU_DEST%\" >nul || exit /b 1
    powershell -NoProfile -Command "Expand-Archive -Path '%CD%\%ZIP%' -DestinationPath '%MCU_DEST%\smart_hub_ui' -Force" || exit /b 1
    echo Extracted to: %MCU_DEST%\smart_hub_ui\
)

:cleanup
if exist "%STAGE%" rmdir /s /q "%STAGE%"
endlocal
