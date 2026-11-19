# Smart Studio UI simulator (PC / Windows)

Runs the LVGL v8.4 UI natively on Windows with GDI, under the MCU profile:
480 x 320 RGB565, 48 KiB LVGL heap, one 480 x 20 partial draw buffer.

## Real size

The window opens at the panel's physical size, 73.4 x 49.0 mm (the 3.5-inch
active area), using in order:

1. `--ppi N` on the command line;
2. a calibration saved for this monitor resolution in `sim_view.txt` next to
   the executable;
3. the pixel density Windows reports for the monitor;
4. 96 ppi.

Hold the LCD module or a ruler to the screen. If the picture is not 73 mm wide,
press `+` or `-` until it is; the value is saved for the current resolution, so
a Remote Desktop session and the local console keep separate values. Over
Remote Desktop Windows reports the remote session's monitor, not the screen in
front of you, so calibrate once there. The panel has about 166 ppi and a
desktop monitor fewer, so the real-size picture is softer than the LCD. Use
`V` for pixel 1:1 and 2:1 views to inspect detail.

## Keys

| Key | Action |
|---|---|
| `1`–`4` | Light, Air conditioner, Temperature, Humidity |
| `V` | Real size → pixel 1:1 → pixel 2:1 |
| `+` / `-` | Calibrate the real-size view (saved per monitor resolution) |
| `H` / `S` / `A` | Home / Sleep / Away scene sent "from the PC" |
| `E` | Room readings available / unavailable |
| `F` | Make the next baud request fail |
| `Esc` | Quit |

Touch actions are printed as the JSON events the firmware would send. A baud
request completes after 1.2 s.

## Headless modes

```powershell
sim_pc\build\sim_pc.exe --smoke           # first frame through the partial buffer
sim_pc\build\sim_pc.exe --regression      # full regression suite
sim_pc\build\sim_pc.exe --shot <file.raw> # Light page, Home scene
sim_pc\build\sim_pc.exe --shots <dir>     # 19 named scenario frames
sim_pc\build\smoke_production.exe         # exported sources, strict C99, no hooks
```

Convert `.raw` framebuffer dumps into 480 x 320 PNGs with
`python tools/raw2png.py <in.raw> <out.png>` (used for `docs/screenshots/`).
