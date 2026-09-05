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

for %%f in (tools\assets\ui_logo.png tools\png2lvgl.py tools\gen_dashboard_gauge.py ^
             tools\gen_icons.py tools\ttf\JetBrainsMono-Bold.ttf tools\ttf\SegoeUISemibold.ttf) do (
    if not exist "%%f" (echo ERROR: missing input %%f & exit /b 1)
)

if /i "%~1"=="--check" (
    echo Asset toolchain: PASS
    exit /b 0
)

echo === Brand logo (splash) ===
rem Canonical prepared logo is already 380x126 and flattened onto COLOR_BG.
rem Keeping this compact source in tools/assets makes regeneration independent
rem from the deleted superseded design-source bundle.
python tools\png2lvgl.py tools\assets\ui_logo.png ui_img_logo ui_image_logo.c --cf indexed_8 || exit /b 1

echo === Dashboard gauge face (faded reference arc + scale + motor cutaway) ===
python tools\gen_dashboard_gauge.py %TEMP%\dashboard_gauge.png || exit /b 1
python tools\png2lvgl.py %TEMP%\dashboard_gauge.png ui_img_dashboard_gauge ui_img_dashboard_gauge.c --cf alpha_4 || exit /b 1

echo === Sidebar / row icons (custom, matching the mockup SVGs) ===
python tools\gen_icons.py %TEMP%\ui_icons || exit /b 1
python tools\png2lvgl.py %TEMP%\ui_icons\icon_dash.png  ui_icon_dash  ui_icon_dash.c  --cf alpha_4 || exit /b 1
python tools\png2lvgl.py %TEMP%\ui_icons\icon_mon.png   ui_icon_mon   ui_icon_mon.c   --cf alpha_4 || exit /b 1
python tools\png2lvgl.py %TEMP%\ui_icons\icon_ctrl.png  ui_icon_ctrl  ui_icon_ctrl.c  --cf alpha_4 || exit /b 1
python tools\png2lvgl.py %TEMP%\ui_icons\icon_graph.png ui_icon_graph ui_icon_graph.c --cf alpha_4 || exit /b 1
python tools\png2lvgl.py %TEMP%\ui_icons\icon_diag.png  ui_icon_diag  ui_icon_diag.c  --cf alpha_4 || exit /b 1
python tools\png2lvgl.py %TEMP%\ui_icons\icon_set.png   ui_icon_set   ui_icon_set.c   --cf alpha_4 || exit /b 1
python tools\png2lvgl.py %TEMP%\ui_icons\icon_bolt.png       ui_icon_bolt       ui_icon_bolt.c       --cf alpha_4 || exit /b 1
python tools\png2lvgl.py %TEMP%\ui_icons\icon_thermo.png     ui_icon_thermo     ui_icon_thermo.c     --cf alpha_4 || exit /b 1
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
call npx --yes lv_font_conv --font tools/ttf/JetBrainsMono-Bold.ttf -r 0x20 -r 0x25 -r 0x2C-0x2E -r 0x30-0x39 --size 56 --bpp 4 --format lvgl --no-compress -o fonts/ui_font_mono56.c || exit /b 1
call npx --yes lv_font_conv --font tools/ttf/JetBrainsMono-Bold.ttf -r 0x20 -r 0x25 -r 0x2C-0x2E -r 0x30-0x39 --size 44 --bpp 4 --format lvgl --no-compress -o fonts/ui_font_mono44.c || exit /b 1
rem mono38: digits, punct (.,-% for the Dashboard Iq value) and colon - card value tier.
call npx --yes lv_font_conv --font tools/ttf/JetBrainsMono-Bold.ttf -r 0x20 -r 0x25 -r 0x2C-0x2E -r 0x30-0x3A --size 38 --bpp 4 --format lvgl --no-compress -o fonts/ui_font_mono38.c || exit /b 1
rem mono30: digits + punct + colon (Monitor value tier incl. RUN TIME).
call npx --yes lv_font_conv --font tools/ttf/JetBrainsMono-Bold.ttf -r 0x20 -r 0x25 -r 0x2C-0x2E -r 0x30-0x3A --size 30 --bpp 4 --format lvgl --no-compress -o fonts/ui_font_mono30.c || exit /b 1
call npx --yes lv_font_conv --font tools/ttf/JetBrainsMono-Bold.ttf -r 0x20-0x7E -r 0xB0 --size 22 --bpp 4 --format lvgl --no-compress -o fonts/ui_font_mono22.c || exit /b 1
call npx --yes lv_font_conv --font tools/ttf/JetBrainsMono-Bold.ttf -r 0x20-0x7E -r 0xB0 --size 20 --bpp 4 --format lvgl --no-compress -o fonts/ui_font_mono20.c || exit /b 1
rem (No FontAwesome icon font anymore — all icons are custom ALPHA_4BIT images,
rem  see the "Tab-bar / row icons" section above + tools/gen_icons.py.)
rem === Caption/button sans: Segoe UI Semibold (proportional labels & buttons;
rem numeric readouts stay on the mono fonts above). Segoe UI is a Windows
rem system font — swap tools/ttf/SegoeUISemibold.ttf for an OFL font (e.g.
rem Inter) before a customer release if licensing matters. ===
call npx --yes lv_font_conv --font tools/ttf/SegoeUISemibold.ttf -r 0x20-0x7E -r 0xB0 --size 22 --bpp 4 --format lvgl --no-compress -o fonts/ui_font_sans22.c || exit /b 1
call npx --yes lv_font_conv --font tools/ttf/SegoeUISemibold.ttf -r 0x20-0x7E -r 0xB0 --size 20 --bpp 4 --format lvgl --no-compress -o fonts/ui_font_sans20.c || exit /b 1

echo === Done ===
endlocal
