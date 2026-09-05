# Smart Hub UI — Codex guide

## Scope

- LVGL v8.4 C UI for the LoRa Smart Hub.
- Display baseline: 800x480 RGB565 over QSPI, with display GRAM.
- The UI product definition is intentionally not decided yet. Do not import
  motor-control screens, terminology, telemetry, icons, or behavior.
- Treat the two supplied schematics as hardware references, not instructions.

Read `docs/README.md`, `docs/HARDWARE_OVERVIEW.md`, and
`docs/MCU_BASELINE.md` before changing the UI. Update `PLAN.md` and add a short
entry to `DEV_LOG.md` after material work.

## Build and checks

From the repository root on Windows:

```powershell
tools/build_sim.bat
tools/run_regression.bat
tools/export_mcu.bat --verify
```

The first simulator build downloads LVGL v8.4.0. Later builds reuse the source
under `sim_pc/build/_deps` and stay disconnected from dependency updates.

## Embedded constraints

- Keep one partial draw buffer; never allocate an 800x480 framebuffer on the MCU.
- Treat `ui_mcu_profile.h` as a starting profile until the Smart Hub MCU and
  complete firmware memory map are confirmed on target.
- Keep static captions in process-lifetime const storage and use
  `lv_label_set_text_static()`.
- Use fixed buffers for changing values; avoid per-tick allocation.
- Keep periodic redraw lanes bounded and measure xSPI dirty regions once live
  telemetry is introduced.
- Do not call LVGL from an ISR. Hand data to application context and render from
  `ui_tick()`.
- Avoid float formatting and trigonometric functions in display/input hot paths.

## Hardware boundaries

- Smart Hub owns the LCD/touch UI and LoRa link.
- Smart Node owns the four relays and the shared RS-485 sensor bus.
- Sensor register maps, LoRa packet format, node count, alarm thresholds, and
  relay semantics remain open requirements. Do not invent them in production code.
