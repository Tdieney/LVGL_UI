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

for %%f in (lora_comm.h lora_hub_link.h LORA_PROTOCOL.md ui.c ui.h ui_auto.h ui_auto.c ui_internal.h ui_mcu_profile.h ui_types.h ui_theme.h ui_theme.c ui_icons.h ui_icons.c ui_fonts.h ui_fonts.c ui_splash_logo.c) do (
    copy /y "%%f" "%STAGE%\" >nul || (echo ERROR: missing %%f & exit /b 1)
)
for %%f in (HARDWARE_OVERVIEW.md MCU_BASELINE.md README.md UI_DESIGN_BRIEF.md) do (
    copy /y "docs\%%f" "%STAGE%\docs\" >nul || (echo ERROR: missing docs\%%f & exit /b 1)
)

if defined VERIFY_ONLY (
    echo Smart Hub MCU export manifest: PASS
    goto :cleanup
)

set "EXPLICIT_ZIP=%~2"
if not "%EXPLICIT_ZIP%"=="" (
    set "OUT_ZIP=%EXPLICIT_ZIP%"
) else (
    for /f %%i in ('powershell -NoProfile -Command "Get-Date -Format yyyyMMdd_HHmm"') do set "TS=%%i"
    set "OUT_ZIP=%CD%\smart_hub_ui_%TS%.zip"
    if exist "%CD%\smart_hub_ui_%TS%.zip" (
        for /l %%c in (1,1,999) do (
            if not exist "%CD%\smart_hub_ui_%TS%_%%c.zip" (
                set "OUT_ZIP=%CD%\smart_hub_ui_%TS%_%%c.zip"
                goto :zip_selected
            )
        )
    )
)
:zip_selected
powershell -NoProfile -Command "Compress-Archive -Path '%STAGE%\*' -DestinationPath '%OUT_ZIP%' -Force" || exit /b 1
echo Created: %OUT_ZIP%

if not "%MCU_DEST%"=="" (
    if not exist "%MCU_DEST%" mkdir "%MCU_DEST%" || exit /b 1
    copy /y "%OUT_ZIP%" "%MCU_DEST%\" >nul || exit /b 1
    powershell -NoProfile -Command "Expand-Archive -Path '%OUT_ZIP%' -DestinationPath '%MCU_DEST%\smart_hub_ui' -Force" || exit /b 1
    echo Extracted to: %MCU_DEST%\smart_hub_ui\
)

:cleanup
if exist "%STAGE%" rmdir /s /q "%STAGE%"
endlocal
