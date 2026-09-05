# UI tools

This directory is part of the `ui_7inch` repository. Every script resolves the
repository root from its own location, so it can be launched from any working
directory.

## Daily commands

```powershell
# Configure and build the Windows simulator
tools/build_sim.bat

# Full source, position, Demo/Monitor stress and six-screen heap regression
tools/run_regression.bat

# Reuse an already-built simulator
tools/run_regression.bat -SkipBuild

# Validate the asset dependencies and input files without regenerating assets
tools/build_assets.bat --check

# Validate the MCU export manifest without creating an archive
tools/export_mcu.bat --verify

# Capture all six tabs to PNG + one heap summary for visual review
tools/shoot_all.bat
```

### Visual / heap review: `shoot_all.bat`

Runs the headless simulator once per tab (`--shot`), converts each RGB565 dump
to a PNG, and writes a single `heap_summary.txt` with every tab's LVGL heap
report. Default output is `docs\screenshots` — the same place the documented
reference shots live — so a quick review pass is also a reference refresh.

```powershell
# Demo telemetry on, motor stopped (matches the docs reference shots)
tools/shoot_all.bat

# Motor spinning ~83% max RPM: livelier screens for reviewing a change
tools/shoot_all.bat -Telemetry demo-run

# No demo telemetry (disconnected, all-zero readouts)
tools/shoot_all.bat -Telemetry none

# Keep a working copy instead of touching docs\screenshots
tools/shoot_all.bat -OutDir C:\tmp\shots

# Reuse an already-built simulator
tools/shoot_all.bat -SkipBuild

# Longer settle time per tab (default 10000 ms of simulated UI time)
tools/shoot_all.bat -Ms 15000
```

Requires the simulator built (see `build_sim.bat`) and Python with Pillow
(`raw2png.py`). Any failed tab returns a non-zero exit code, so it can gate a
pre-commit/CI check. The `heap_summary.txt` numbers are the same measured
values documented in `docs/MCU_MEMORY_PROFILE.md`.

### Widget tree dump: `sim_pc.exe --layout <tab>`

Text "vision" for layout debugging without looking at pixels — prints the live
widget tree after the tab settles: class, effective visibility (any ancestor
`HIDDEN` marks the whole subtree), absolute coordinates/size, plus label text
and slider/arc/bar/dropdown values. Overlaps, clipping and offset bugs are
easier to diff in text than in a PNG, and it works in CI.

```powershell
sim_pc\build\sim_pc.exe --layout control --ms 4000 --demo
sim_pc\build\sim_pc.exe --layout control --mode torque --ms 4000 --demo-run
sim_pc\build\sim_pc.exe --layout control --mode position --ms 4000 --demo
```

Output is grouped by layer: `== layer_top ==` (persistent top bar + sidebar),
`== screen ==` (the active tab) and `== layer_sys ==`. The same `--demo` /
`--demo-run` / `--mode` / `--ms` options apply as `--shot`; `--ms` accepts
1..600000 ms of virtual time.

### xSPI redraw profile: `sim_pc.exe --profile <tab>`

Đếm đúng số pixel LVGL gửi qua `flush_cb` sau khi boot/splash/tab đã settle. Vì draw
buffer MCU chỉ cao 10 dòng, report tách `frames` (một refresh batch) và `calls`
(các transfer chunk); `pixels`/`px/s` là tải bus cần so sánh.

```powershell
sim_pc\build\sim_pc.exe --profile graphs --ms 10000 --demo-run
sim_pc\build\sim_pc.exe --profile control --mode position --ms 10000 --demo-run --drag
```

`--drag` chỉ hợp lệ với Control/POSITION và chạy một vòng touch xác định qua input
driver. Full regression khóa budget cho Control SPEED/TORQUE, Graphs và đường kéo
này để tránh vô tình khôi phục redraw burst lớn/full-tile invalidation.

`--profile-switch <from> <to>` đo đúng một lần đổi tab sau khi tab nguồn
đã settle. Report tách `chrome-frame` (icon cũ/mới + title) khỏi `max-frame`
(content), không lẫn periodic telemetry. Dùng để bắt regression quay lại
full-screen 800x480 hoặc vô tì gom chrome/content vào một batch:

```powershell
sim_pc\build\sim_pc.exe --profile-switch dashboard control --demo
```

Baseline cũ là 384,000 px / 768,000 byte / 48 calls cho mọi cặp tab. Regression chạy
đủ vòng sáu tab và khóa total ≤340k px, chrome ≤22k px, content burst
≤315k px, `frames=2`.

`--test-nav` bơm pointer press/release qua input driver vào tâm cả sáu cell của rail,
đi qua callback/hit-map thật và xác nhận `ui_current_tab`. Regression luôn chạy test
này để bắt lỗi lệch vùng click sau khi navigation được gom thành một custom widget.

`--test-dashboard-bands` drive Dashboard qua SLOW tick với 169/170/201 RPM, wire
fault và recovery để khóa cả band lẫn màu SPEED arc
signal-cyan/amber/danger mà không phụ thuộc
vào việc đọc màu thủ công từ screenshot.

The `.bat` entrypoints launch the matching PowerShell implementation with a
process-local execution-policy bypass; they do not change the machine policy.
`run_regression.ps1` uses a unique temporary directory and removes its raw
screenshots when it finishes. Its default stress duration is 120,000 ms of
simulated UI time.

## Assets

Run `tools/build_assets.bat` only when intentionally regenerating the logo,
watermark, icons or fonts. It requires Python 3, Pillow, Node.js and `npx`.
Generated `ui_icon_*.c`, `ui_image_*.c`, `ui_img_*.c` and `fonts/*.c` files are
firmware assets; review their size changes before committing them.

`tools/assets/ui_logo.png` is the compact canonical 380x126 splash-logo source,
already flattened onto `COLOR_BG`; `build_assets.bat` converts it directly to
the indexed-8 `ui_image_logo.c` asset.

`gen_dashboard_gauge.py` renders the 320x320 layered Alpha-4 Dashboard face:
an extended gray reference arc with faded tails, legible scale labels, ticks,
inner ring and a detailed faint BLDC cutaway. The live LVGL layer updates only
the green speed arc, RPM digits and direction icon.

## MCU handoff

`tools/export_mcu.bat` packages the UI source, fonts, assets,
`ui_mcu_profile.h`, the UART protocol and MCU memory notes. With an optional
destination it also expands the package into `<destination>/hmi_ui`.

After linking the target, report static Flash/RAM usage with the matching GNU
toolchain's `size` executable:

```powershell
tools/mcu_size.bat -Elf path/to/firmware.elf
tools/mcu_size.bat -Elf firmware.elf -SizeTool riscv-none-elf-size
```

The size report cannot see runtime stack peaks. Record stack high-water and
LVGL heap telemetry on hardware before a customer demo.
