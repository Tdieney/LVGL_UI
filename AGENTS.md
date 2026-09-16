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
- **Auto scope (Owner request, 2026-09-14):** The next implementation handoff includes Hub-side Auto with per-device ON/OFF thresholds editable on the LCD, replacing the earlier fixed-threshold-only proposal. The technical-lead v1 specification covers Ventilation Fan/CO2, Air Purifier/VOC and Humidifier/RH, with validation, hysteresis, manual override and explicit loss/recovery behavior; see `docs/prompts/CURRENT.md`. This does not authorize Node-side automation, radio drivers, Flash persistence drivers or a packet-schema change. The feature is specified, not yet implemented.
