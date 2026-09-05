# 7-inch HMI — Codex guide

## Scope and target

- LVGL v8.4 C UI for an 800x480 RGB565 HMI.
- Target: 80 MHz MCU, 1 MB Flash, 128 KB RAM, xSPI display with its own GRAM.
- The xSPI dirty-region rate and SRAM headroom are product constraints, not
  simulator-only concerns.
- `Industrial HMI for BLDC Motor/` is a visual reference. Do not extend it as
  part of firmware work.

Read `docs/README.md` (documentation index) and `docs/MCU_MEMORY_PROFILE.md` before
changing widgets, update `PLAN.md` after material work, log every material change in
`DEV_LOG.md` (root), and treat current source plus this file as authoritative when an
older note in `CLAUDE.md` or `PERF.md` disagrees.

## Build and regression

From the repository root on Windows:

```powershell
tools/build_sim.bat
```

The CMake project keeps an already-populated LVGL dependency offline. Useful
checks after UI changes:

```powershell
tools/run_regression.bat
# Or reuse an existing simulator build:
tools/run_regression.bat -SkipBuild
```

Run headless shots for all six tabs and record `used`, `free`, biggest block and
fragmentation. The validated 42 KB heap limits are documented in
`docs/MCU_MEMORY_PROFILE.md`.

Asset generation, MCU export and target size reporting are documented in
`tools/README.md`. Use `tools/export_mcu.bat --verify` after changing the MCU
source manifest.

## Load-bearing architecture

- One content screen is resident. A tab request is coalesced; teardown/rebuild
  happens from `ui_tick()`, never from the click callback.
- Top and tab chrome live on `lv_layer_top()` and remain resident.
- Every per-screen tick must guard `ui_current_tab` before using static widget
  pointers. The low-memory fallback suppresses all screen ticks.
- Static captions use `lv_label_set_text_static()` and therefore must point to
  string literals or other process-lifetime const storage.
- Dynamic numeric labels must be created with `label_bind_buffer()` and updated
  through `label_set_if_changed()`. Do not reintroduce per-tick label
  allocation with `lv_label_set_text()`.
- Periodic rendering belongs to an existing lane. Monitor deliberately updates
  at most four values per settled SLOW tick; do not turn it back into an
  eight-label burst.
- Production UI math is integer/fixed-point. Do not add float formatting,
  `sinf`, `cosf`, or `atan2f` to a tick/input hot path.

## Telemetry and Demo mode

- `motorStatusFast`/`motorStatusSlow` are real UART-owned frames.
- Demo telemetry lives in private structs in `demo_sim.c`.
- UI code reads telemetry only through `ui_motor_status_fast()`,
  `ui_motor_status_slow()` and `ui_motor_connected()`.
- The real UART receiver may continue running while Demo is active; it no
  longer needs to gate writes on `demo_sim_active`.
- UI actions write `motorCmd`. State displayed to the operator comes from the
  selected status source, not command echo assumptions.
- Demo enable starts at a fully cleared STOP/zero-setpoint/zero-limit state.
  Demo disable also commands STOP but does not erase the latest real UART frame.
- Never call LVGL from an ISR. Commit frames in application context or a safe
  handoff and let `ui_tick()` render them.

## Memory and assets

- Include `ui_mcu_profile.h` from the target configuration. Validated values:
  42 KB LVGL heap and one 800x10 RGB565 draw buffer (16,000 bytes).
- Do not allocate a full 800x480 framebuffer on the MCU; the display has GRAM.
- Keep image maps, image descriptors, font bitmaps/descriptors, lookup tables
  and arrays of pointers to literals `const` so the linker places them in Flash.
- Generated `ui_icon_*.c`, `ui_image_*.c`, `ui_img_*.c` and `fonts/*.c` are
  assets, not normal hand-edited source. Preserve their `const` qualifiers.
- A hidden widget still consumes heap. Reuse a layout when modes share a
  structure; do not keep all six tabs resident.

## MCU handoff checklist

1. Apply `ui_mcu_profile.h` to the firmware's `lv_conf.h` and display driver.
2. Build the target and inspect the linker map, not only the IDE percentage.
3. Measure stack high-water mark and LVGL heap at runtime on Control and during
   Demo START.
4. Verify DMA flush calls `lv_disp_flush_ready()` only after the transfer is
   complete and that LVGL APIs run in one execution context.
5. Run the simulator stress and position tests before a customer build.
