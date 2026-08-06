@echo off
rem ===========================================================================
rem Dong goi cac file UI can thiet va dua sang project MCU.
rem
rem Cach dung (chay tu bat ky dau):
rem   tools\export_mcu.bat                    ^> chi tao file zip tai repo
rem   tools\export_mcu.bat D:\MCU\MyProject   ^> tao zip + copy + giai nen
rem                                             vao D:\MCU\MyProject\hmi_ui\
rem Co the sua san MCU_DEST ben duoi thay vi truyen tham so.
rem
rem Noi dung goi: ui/screens/actions/motor_comm_protocol/demo_sim/images,
rem ui_mcu_profile.h, ui_image_*.c, fonts\*.c va tai lieu tich hop.
rem Dung --verify de kiem tra danh sach file ma khong tao zip.
rem ===========================================================================
setlocal EnableExtensions
cd /d "%~dp0.." || exit /b 1

rem === Duong dan project MCU mac dinh (de trong = chi tao zip) ===
set "VERIFY_ONLY="
if /i "%~1"=="--verify" (
    set "VERIFY_ONLY=1"
    set "MCU_DEST="
) else (
    set "MCU_DEST=%~1"
)

set "STAGE=%TEMP%\hmi_ui_export_%RANDOM%_%RANDOM%"

echo === Gom file ===
if exist "%STAGE%" (echo LOI: trung thu muc tam "%STAGE%" & exit /b 1)
mkdir "%STAGE%\fonts"
mkdir "%STAGE%\docs"

for %%f in (ui.c ui.h screens.c screens.h actions.c actions.h motor_comm_protocol.h ctrl_pos_math.h ^
            demo_sim.c demo_sim.h ui_mcu_profile.h images.h ui_image_logo.c ui_img_motor.c ^
            ui_icon_dash.c ui_icon_mon.c ui_icon_ctrl.c ui_icon_graph.c ui_icon_diag.c ui_icon_set.c ^
            ui_icon_bolt.c ui_icon_battery.c ui_icon_power.c ui_icon_eff.c ui_icon_thermo.c ^
            ui_icon_crosshairs.c ui_icon_encoder.c ui_icon_link.c ui_icon_check.c ui_icon_xmark.c ^
            ui_icon_chevron.c ui_icon_cw.c ui_icon_ccw.c ^
            PERF.md UART_PROTOCOL.md) do (
    copy /y "%%f" "%STAGE%\" >nul || (echo LOI: thieu file %%f & exit /b 1)
)
copy /y "fonts\ui_fonts.h" "%STAGE%\fonts\" >nul || (echo LOI: thieu fonts\ui_fonts.h & exit /b 1)
copy /y "fonts\ui_font_*.c" "%STAGE%\fonts\" >nul || (echo LOI: thieu fonts\ui_font_*.c & exit /b 1)
copy /y "docs\MCU_MEMORY_PROFILE.md" "%STAGE%\docs\" >nul || (echo LOI: thieu docs\MCU_MEMORY_PROFILE.md & exit /b 1)

if defined VERIFY_ONLY (
    echo MCU export manifest: PASS
    goto :cleanup
)

echo === Nen zip ===
for /f %%i in ('powershell -NoProfile -Command "Get-Date -Format yyyyMMdd_HHmm"') do set "TS=%%i"
set "ZIP=hmi_ui_%TS%.zip"
powershell -NoProfile -Command "Compress-Archive -Path '%STAGE%\*' -DestinationPath '%CD%\%ZIP%' -Force" || exit /b 1
echo Da tao: %CD%\%ZIP%

if "%MCU_DEST%"=="" (
    echo.
    echo Chua co duong dan project MCU - dung o buoc tao zip.
    echo Copy tu dong: tools\export_mcu.bat D:\duong\dan\project_mcu
    goto :notes
)
if not exist "%MCU_DEST%" (
    echo LOI: thu muc "%MCU_DEST%" khong ton tai.
    exit /b 1
)

echo === Dua sang project MCU ===
copy /y "%CD%\%ZIP%" "%MCU_DEST%\" >nul || exit /b 1
powershell -NoProfile -Command "Expand-Archive -Path '%CD%\%ZIP%' -DestinationPath '%MCU_DEST%\hmi_ui' -Force" || exit /b 1
echo Da copy zip + giai nen vao: %MCU_DEST%\hmi_ui\

:notes
echo.
echo Nhac tich hop (chi tiet trong PERF.md ben trong goi):
echo  - Them tat ca file .c (goc + fonts\) vao build firmware
echo  - Dinh nghia LV_LVGL_H_INCLUDE_SIMPLE o cap project
echo  - Include ui_mcu_profile.h trong lv_conf.h va display driver
echo  - LV_MEM_SIZE = UI_LVGL_HEAP_BYTES ^(52KB^)
echo  - Mot draw buffer UI_DRAW_BUF_PIXELS ^(800x10 RGB565 = 16000 bytes^)
echo  - Demo khong can motor: bien dich voi -DUI_DEMO_SIM=1

:cleanup
if exist "%STAGE%" rmdir /s /q "%STAGE%"
endlocal
