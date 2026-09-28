# LoRa Smart Hub UI

LoRa Smart Hub UI design workspace with an LVGL v8.4 simulator baseline, MCU
constraints, reusable tools and a reviewable 800x480 product prototype.

## Open the UI design

Run `sim_pc\build\sim_pc.exe` to use the current Quiet Architecture UI:
Overview, Trends, Devices, Settings and LCD-editable Auto limits.
The owner accepted the native appearance on 2026-09-21. On 2026-09-25 the owner
changed the code: Trends is simplified (no Current/Min/Max or tooltip), telemetry
shows eCO₂ ppm / TVOC ppb, and LoRa uses the v2 request/response protocol. See
[current product decisions](docs/UI_DESIGN_BRIEF.md) and
[`LORA_PROTOCOL.md`](LORA_PROTOCOL.md).
The [browser prototype](docs/prototype/index.html) remains a historical design,
not the geometry or styling authority for the current LVGL implementation.

Use the [current implementation/fix prompt](docs/prompts/CURRENT.md) to hand off
coding and the [technical-lead review contract](docs/TECHNICAL_LEAD_REVIEW.md)
to prepare the implementation for review.
All prompt authors must follow [prompt storage rules](docs/prompts/README.md);
historical handoffs are archived there, not in individual review folders.

See [prototype notes and screenshots](docs/prototype/README.md) and the
[inspection/redesign review](docs/UI_REDESIGN_REVIEW.md). The C/LVGL simulator
implements LCD-configurable Auto. Host software was independently accepted on
2026-09-25 before the owner's changes listed above; those changes are not yet
covered by the simulator tests.

## Quick start (Windows)

> **Current status (2026-09-25):** Simulator and tests updated for LoRa v2 protocol
> (`tvoc_ppb`, `sample_seq`-gated Auto, `GET_STATUS` request/response, schema v3) and
> owner-decided Trends refinements (restored hero indicator, 15-min fixed window,
> round-number Y ticks). `tools/build_sim.bat`, `tools/run_regression.bat` (42/42 checks),
> `tools/export_mcu.bat --verify`, and `tools/verify_production.bat` pass cleanly.

```powershell
tools/build_sim.bat
tools/run_regression.bat
sim_pc\build\sim_pc.exe
```

When running `sim_pc.exe` interactively, a deterministic fake node responds to relay and settings commands with a 700 ms acknowledgement delay. Interactive keyboard controls:
- `S`: Replay splash animation.
- `F`: Arm next command to fail (negative ACK after 700 ms -> `Unknown` + `Retry`).
- `T`: Arm next command to timeout (dropped ACK -> times out after 3000 ms -> `Unknown` + `Retry`).
- `1` / `2` / `3`: Navigate directly to Home / Trends / Devices.
- `Esc`: Exit simulator.

The first build needs network access to fetch LVGL v8.4.0; subsequent builds use
the cached source in `sim_pc/build/_deps`.

## Project references

- [Documentation index](docs/README.md)
- [Hardware overview and pin map](docs/HARDWARE_OVERVIEW.md)
- [MCU/UI memory baseline](docs/MCU_BASELINE.md)
- [Discovery and implementation plan](PLAN.md)
- [Development log](DEV_LOG.md)

The completed BLDC motor-control project remains on branch `ui/motor-control`.
