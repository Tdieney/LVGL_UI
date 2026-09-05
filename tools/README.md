# Smart Hub UI tools

## Simulator

```powershell
tools/build_sim.bat
tools/run_regression.bat
sim_pc\build\sim_pc.exe
```

The first build fetches LVGL v8.4.0. `build_sim.ps1` reuses the populated
`sim_pc/build/_deps/lvgl-src` directory without updating it.

Headless checks are also available directly:

```powershell
sim_pc\build\sim_pc.exe --smoke
sim_pc\build\sim_pc.exe --shot smart_hub.raw
python tools/raw2png.py smart_hub.raw smart_hub.png
```

## MCU handoff

```powershell
tools/export_mcu.bat --verify
tools/export_mcu.bat
tools/export_mcu.bat D:\path\to\firmware
```

The package is intentionally small while product discovery is open. Update its
explicit manifest whenever production source/assets are added.

After linking target firmware:

```powershell
tools/mcu_size.bat -Elf path\to\firmware.elf
```

The report covers static Flash/RAM only; measure stack high-water and LVGL heap
on the device.
