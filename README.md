# LoRa Smart Hub UI

Clean LVGL v8.4 starting point for the Smart Hub display. The repository is now
deliberately limited to an 800x480 simulator, an MCU memory baseline, reusable
conversion/size tools, and hardware discovery notes. Product screens and
interaction flows have not been designed yet.

## Quick start (Windows)

```powershell
tools/build_sim.bat
tools/run_regression.bat
sim_pc\build\sim_pc.exe
```

The first build needs network access to fetch LVGL v8.4.0; subsequent builds use
the cached source in `sim_pc/build/_deps`.

## Project references

- [Documentation index](docs/README.md)
- [Hardware overview and pin map](docs/HARDWARE_OVERVIEW.md)
- [MCU/UI memory baseline](docs/MCU_BASELINE.md)
- [Discovery and implementation plan](PLAN.md)
- [Development log](DEV_LOG.md)

The completed BLDC motor-control project remains on branch `ui/motor-control`.
