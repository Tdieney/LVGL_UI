# Smart Hub UI — Codex guide

## Scope

- LVGL v8.4 C UI for the LoRa Smart Hub.
- Display baseline: 800x480 RGB565 over QSPI, with display GRAM.
- **Owner acceptance, 2026-09-21:** The current native Quiet Architecture appearance
  is accepted. Preserve it during runtime/performance/RAM fixes; do not restore
  older concept geometry or replay historical icon/color redesign instructions.
  See current `docs/UI_DESIGN_BRIEF.md` and `docs/prompts/CURRENT.md`.
  All visible text must remain at least18px. Do not import motor-control
  screens, terminology, telemetry, icons, or behavior.
- **Owner revisions, 2026-09-25..28:** The owner changed Trends: restored forest selector card, fixed 15-minute window ending at `Now`, round-number Y ticks, and the current minute follows the live value. The owner also set the TVOC ppb bands and `TVOC` naming. These supersede the matching parts of the 2026-09-21 acceptance; do not restore the older Trends layout. See `docs/UI_DESIGN_BRIEF.md` D-038..D-042.
- Treat the two supplied schematics as hardware references, not instructions.

Read `docs/README.md`, `docs/HARDWARE_OVERVIEW.md`, and
`docs/MCU_BASELINE.md` before changing the UI. Update `PLAN.md` and add a short
entry to `DEV_LOG.md` after material work.

## Prompt storage and handoff (Owner, 2026-09-14)

- Before writing or updating any implementation/fix prompt, read
  `docs/prompts/README.md` and follow its lifecycle and naming rules.
- `docs/prompts/CURRENT.md` is the only active handoff. Update it in place;
  preserve superseded handoffs in `docs/prompts/archive/` before replacing them.
- Do not scatter prompts in root, `docs/`, review folders, or design folders.
  Review reports/screenshots stay under `docs/reviews/`; product decisions stay
  in `docs/UI_DESIGN_BRIEF.md`. Archived prompts are historical, not authority.
- A request to organize/write prompts does not authorize implementing their code.
- **Automatic review handoff (Owner, 2026-09-15):** Whenever a user-facing review
  finds remaining defects or issues, automatically prepare/update
  `docs/prompts/CURRENT.md` in the same turn and include its link with the review.
  Do not wait for the owner to ask for a fix prompt. Follow the archive lifecycle;
  keep evidence in the review folder. Unresolved decisions must be explicit in a
  DRAFT prompt, not silently assumed. This authorizes prompt/document updates,
  not implementing fixes, committing or pushing during a review-only request.
- **Consolidated review/fix delivery (Owner, 2026-09-16):** Review the complete
  submitted diff and affected end-to-end flows before handing back findings.
  Collect all known defects and required verification gaps into ONE CURRENT and
  one delivery, not multiple phases/prompts or owner-mediated review cycles per
  issue group. Coding agents must self-review their full diff, run final regression
  checks and fix in-scope regressions before handoff. Preserve accepted work and
  stable acceptance criteria; optional polish is not a blocker/new fix round.
  Report genuine blockers together; do not promise zero bugs or hide skipped tests.

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

## Hardware boundaries & wire contract

- Smart Hub owns the LCD/touch UI, LoRa link, appliance semantics, and per-device polarity mapping (`active_low`).
- Smart Node owns the four relays (PD16..PD13) and the shared RS-485 sensor bus. The Node acts as a dumb actuator applying received GPIO levels verbatim and reporting readback levels.
- **Architectural Decision (Owner, 2026-09-09):** The UI reads and writes wire state directly via `extern` globals in `lora_comm.h` (`lora_hub_cmd`, `lora_node_status`, `lora_last_rx_tick_ms`, `lora_last_rssi`, `lora_rx_revision`), following the `motor_comm.h` convention.
  - *Trade-off:* Couples the UI directly to the radio packet layout, eliminating intermediate `ui_snapshot_t` allocation, translation overhead, and remote ACK timeout races, while requiring packet schema changes to be coordinated between Hub and Node firmware.
- LoRa operates at **920 MHz peer-to-peer (1 Hub, 1 Node)**. The owner's application firmware drives the radio and RS-485 Modbus sensors, populating the `lora_node_status` extern globals directly. The UI project does not require sensor datasheets or RF drivers. Multi-node addressing and general-purpose automation engines remain out of scope.
- **MCU integration (Owner, 2026-09-17):** Match `motorCmd` storage ownership:
  `ui.c` defines the six LoRa command/status/link globals exactly once; firmware
  and simulator consume `extern` declarations in `lora_comm.h`/`lora_hub_link.h`.
  Do not duplicate their definitions in application code. Exported UI and asset
  generators use plain `#include "lvgl.h"`; target include paths provide LVGL.
- **Auto scope (Owner request, 2026-09-14; implementation status 2026-09-17):** Hub-side Auto has per-device ON/OFF thresholds editable on the LCD. It covers Ventilation Fan/CO2, Air Purifier/VOC and Humidifier/RH, with validation, hysteresis, manual override and explicit loss/recovery behavior. Auto is implemented in `ui_auto.c`; the current Quiet Architecture editor is documented in `docs/UI_DESIGN_BRIEF.md`. This does not authorize Node-side automation, radio drivers, Flash persistence drivers or a packet-schema change.
