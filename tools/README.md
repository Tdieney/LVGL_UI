# Smart Studio UI tools

## Simulator

```powershell
tools/build_sim.bat
tools/run_regression.bat
sim_pc\build\sim_pc.exe
```

The first build fetches LVGL v8.4.0. `build_sim.ps1` reuses the populated
`sim_pc/build/_deps/lvgl-src` directory without updating it.

`run_regression` builds, then runs the strict-C99 production smoke test, the
simulator smoke and regression, a single shot, bad-argument checks and the
19 scenario shots.

```powershell
sim_pc\build\sim_pc.exe --shot frame.raw
python tools/raw2png.py frame.raw frame.png
```

## Assets

```powershell
python tools/draw_icons.py
python tools/generate_assets.py --force-icons
python tools/generate_assets.py --check
```

`generate_assets.py` writes `ui_icons.c/.h` from `assets/icons/studio/` and
`ui_fonts.c/.h` from `PlusJakartaSans.ttf` (license in `FONT_LICENSE.txt`). Do
not edit the generated tables by hand.

## MCU export

```powershell
tools/export_mcu.bat --verify
tools/export_mcu.bat
tools/export_mcu.bat D:\path\to\firmware
```

The package holds the portable sources and the firmware documents. Update its
file list when sources are added. After linking the firmware, use `idf.py size`
for static memory and measure stacks and the LVGL heap on the target board.
