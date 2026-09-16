# LoRa Smart Hub UI

LoRa Smart Hub UI design workspace with an LVGL v8.4 simulator baseline, MCU
constraints, reusable tools and a reviewable 800x480 product prototype.

## Open the UI design

Open [docs/prototype/index.html](docs/prototype/index.html) in a browser to try
Home, Trends, Devices and the splash. It works offline without
installation. Use the controls below the device to review sample fault states.

Use the [current implementation/fix prompt](docs/prompts/CURRENT.md) to hand off
coding and the [technical-lead review contract](docs/TECHNICAL_LEAD_REVIEW.md)
to prepare the implementation for review.
All prompt authors must follow [prompt storage rules](docs/prompts/README.md);
historical handoffs are archived there, not in individual review folders.

See [prototype notes and screenshots](docs/prototype/README.md) and the
[inspection/redesign review](docs/UI_REDESIGN_REVIEW.md). The C/LVGL simulator
implements the current design. Corrective work and LCD-configurable Auto described
in the active handoff are specified next work, not completed implementation.

## Quick start (Windows)

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
