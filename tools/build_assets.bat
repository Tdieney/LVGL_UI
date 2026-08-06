@echo off
rem Regenerate all UI assets (dial image + fonts). Run from the repo root:
rem   tools\build_assets.bat
rem Requires: Python 3 + Pillow, Node.js (npx lv_font_conv).
rem Use --check to validate the toolchain and inputs without rewriting assets.

setlocal EnableExtensions
cd /d "%~dp0.." || exit /b 1

where python >nul 2>nul || (echo ERROR: python not found in PATH & exit /b 1)
python -c "import PIL" >nul 2>nul || (echo ERROR: Python package Pillow is not installed & exit /b 1)
where npx >nul 2>nul || (echo ERROR: npx not found in PATH & exit /b 1)

for %%f in (Asset-3.png tools\prep_logo.py tools\png2lvgl.py tools\gen_motor_wm.py ^
             tools\gen_icons.py tools\ttf\JetBrainsMono-Bold.ttf) do (
    if not exist "%%f" (echo ERROR: missing input %%f & exit /b 1)
)

if /i "%~1"=="--check" (
    echo Asset toolchain: PASS
    exit /b 0
)

rem Dial face image removed: the Dashboard gauge is now a clean lv_arc ring
rem (no baked ticks / scale numbers), so no dial image is generated. See
rem tools\gen_dial.py if a pre-rendered ticked face is ever wanted back.

echo === Brand logo (splash) ===
python tools\prep_logo.py Asset-3.png %TEMP%\ui_logo.png || exit /b 1
python tools\png2lvgl.py %TEMP%\ui_logo.png ui_img_logo ui_image_logo.c --cf indexed_8 || exit /b 1

echo === Motor watermark (Dashboard speed tile, E2 cross-section) ===
python tools\gen_motor_wm.py %TEMP%\motor_wm.png || exit /b 1
python tools\png2lvgl.py %TEMP%\motor_wm.png ui_img_motor ui_img_motor.c --cf alpha_4 || exit /b 1

echo === Tab-bar icons (custom, matching the mockup SVGs) ===
python tools\gen_icons.py %TEMP%\ui_icons || exit /b 1
python tools\png2lvgl.py %TEMP%\ui_icons\icon_dash.png  ui_icon_dash  ui_icon_dash.c  --cf alpha_4 || exit /b 1
python tools\png2lvgl.py %TEMP%\ui_icons\icon_mon.png   ui_icon_mon   ui_icon_mon.c   --cf alpha_4 || exit /b 1
python tools\png2lvgl.py %TEMP%\ui_icons\icon_ctrl.png  ui_icon_ctrl  ui_icon_ctrl.c  --cf alpha_4 || exit /b 1
python tools\png2lvgl.py %TEMP%\ui_icons\icon_graph.png ui_icon_graph ui_icon_graph.c --cf alpha_4 || exit /b 1
python tools\png2lvgl.py %TEMP%\ui_icons\icon_diag.png  ui_icon_diag  ui_icon_diag.c  --cf alpha_4 || exit /b 1
python tools\png2lvgl.py %TEMP%\ui_icons\icon_set.png   ui_icon_set   ui_icon_set.c   --cf alpha_4 || exit /b 1
python tools\png2lvgl.py %TEMP%\ui_icons\icon_bolt.png       ui_icon_bolt       ui_icon_bolt.c       --cf alpha_4 || exit /b 1
python tools\png2lvgl.py %TEMP%\ui_icons\icon_battery.png    ui_icon_battery    ui_icon_battery.c    --cf alpha_4 || exit /b 1
python tools\png2lvgl.py %TEMP%\ui_icons\icon_power.png      ui_icon_power      ui_icon_power.c      --cf alpha_4 || exit /b 1
python tools\png2lvgl.py %TEMP%\ui_icons\icon_eff.png        ui_icon_eff        ui_icon_eff.c        --cf alpha_4 || exit /b 1
python tools\png2lvgl.py %TEMP%\ui_icons\icon_thermo.png     ui_icon_thermo     ui_icon_thermo.c     --cf alpha_4 || exit /b 1
python tools\png2lvgl.py %TEMP%\ui_icons\icon_crosshairs.png ui_icon_crosshairs ui_icon_crosshairs.c --cf alpha_4 || exit /b 1
python tools\png2lvgl.py %TEMP%\ui_icons\icon_encoder.png    ui_icon_encoder    ui_icon_encoder.c    --cf alpha_4 || exit /b 1
python tools\png2lvgl.py %TEMP%\ui_icons\icon_link.png       ui_icon_link       ui_icon_link.c       --cf alpha_4 || exit /b 1
python tools\png2lvgl.py %TEMP%\ui_icons\icon_check.png      ui_icon_check      ui_icon_check.c      --cf alpha_4 || exit /b 1
python tools\png2lvgl.py %TEMP%\ui_icons\icon_xmark.png      ui_icon_xmark      ui_icon_xmark.c      --cf alpha_4 || exit /b 1
python tools\png2lvgl.py %TEMP%\ui_icons\icon_chevron.png    ui_icon_chevron    ui_icon_chevron.c    --cf alpha_4 || exit /b 1
python tools\png2lvgl.py %TEMP%\ui_icons\icon_cw.png         ui_icon_cw         ui_icon_cw.c         --cf alpha_4 || exit /b 1
python tools\png2lvgl.py %TEMP%\ui_icons\icon_ccw.png        ui_icon_ccw        ui_icon_ccw.c        --cf alpha_4 || exit /b 1

echo === Fonts ===
rem Type scale bumped for HMI legibility: hero 36->44, value 20->22 (now Bold,
rem was Regular), label tier 12+14 merged into one 20px Bold tier (floor: no
rem UI text below 20px anymore). Delete the old sizes so CMake's fonts/*.c
rem glob does not keep stale unreferenced font tables in the firmware image.
del /q fonts\ui_font_mono36.c fonts\ui_font_mono14.c fonts\ui_font_mono12.c fonts\ui_font_icons16.c fonts\ui_font_icons20.c 2>nul
rem mono66: digits+punct only — Dashboard RPM + Control SPEED/TORQUE hero numbers (~50% bigger than mono44).
call npx --yes lv_font_conv --font tools/ttf/JetBrainsMono-Bold.ttf -r 0x20 -r 0x25 -r 0x2C-0x2E -r 0x30-0x39 --size 66 --bpp 4 --format lvgl --no-compress -o fonts/ui_font_mono66.c || exit /b 1
call npx --yes lv_font_conv --font tools/ttf/JetBrainsMono-Bold.ttf -r 0x20 -r 0x25 -r 0x2C-0x2E -r 0x30-0x39 --size 44 --bpp 4 --format lvgl --no-compress -o fonts/ui_font_mono44.c || exit /b 1
rem mono30: digits+punct only (card readouts POWER/MOTOR TEMP) — matches the mockup's 30px card value tier.
call npx --yes lv_font_conv --font tools/ttf/JetBrainsMono-Bold.ttf -r 0x20 -r 0x25 -r 0x2C-0x2E -r 0x30-0x39 --size 30 --bpp 4 --format lvgl --no-compress -o fonts/ui_font_mono30.c || exit /b 1
call npx --yes lv_font_conv --font tools/ttf/JetBrainsMono-Bold.ttf -r 0x20-0x7E -r 0xB0 --size 22 --bpp 4 --format lvgl --no-compress -o fonts/ui_font_mono22.c || exit /b 1
call npx --yes lv_font_conv --font tools/ttf/JetBrainsMono-Bold.ttf -r 0x20-0x7E -r 0xB0 --size 20 --bpp 4 --format lvgl --no-compress -o fonts/ui_font_mono20.c || exit /b 1
rem (No FontAwesome icon font anymore — all icons are custom ALPHA_4BIT images,
rem  see the "Tab-bar / row icons" section above + tools/gen_icons.py.)

echo === Done ===
endlocal
