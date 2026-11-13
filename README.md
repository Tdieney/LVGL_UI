# Smart Studio UI

LVGL v8.4 touch UI for the ESP32 course Session 08 midterm, "Smart Studio UI":
ESP32-S3, 3.5-inch 480 x 320 ST7796 SPI LCD in RGB565, FT6336 touch. The PC
side is the Web Serial page in [docs/](docs/).

Four swipe pages (Light with a 16-ray radiant sun dial and tap/drag core,
Air conditioner, Temperature, Humidity), a circular Scenes panel (Home, Sleep,
Away) and a Connection baud-stepper panel, in a zero-text high-contrast dark
design. See the [assignment](docs/MIDTERM_ASSIGNMENT.md), the
[design brief](docs/UI_DESIGN_BRIEF.md) and the
[screenshots](docs/screenshots/).

## Quick start (Windows)

```powershell
tools/build_sim.bat
tools/run_regression.bat
sim_pc\build\sim_pc.exe
```

The simulator opens at the panel's real size on your monitor. If it does not
measure 73 mm wide, press `+` or `-` until it does; the setting is kept per
monitor resolution. `V` switches to pixel 1:1 and 2:1 views. Other keys and the
headless modes are in [sim_pc/README.md](sim_pc/README.md).

The first build needs network access to fetch LVGL v8.4.0; later builds use the
cached source in `sim_pc/build/_deps`.

## Layout

| Path | Contents |
|---|---|
| `ui.c`, `ui.h`, `ui_*.c/.h`, `studio_comm.h` | Portable UI, exported to the firmware |
| `sim_pc/` | Windows simulator, regression, production smoke test, `lv_conf.h` |
| `tools/` | Build, regression, export, asset and screenshot scripts |
| `assets/icons/studio/` | Icon masters and manifest |
| `docs/` | Assignment, Web starter, design brief, firmware guide, screenshots |

## Firmware

```powershell
tools/export_mcu.bat --verify
tools/export_mcu.bat D:\path\to\firmware
```

[docs/FIRMWARE_INTEGRATION.md](docs/FIRMWARE_INTEGRATION.md) covers the LVGL
settings, the four calls (`ui_init`, `ui_apply`, `ui_note_rx`,
`ui_baud_complete`), the UART-to-GUI flow and the baud change. The JSON field
mapping is in [studio_comm.h](studio_comm.h).
