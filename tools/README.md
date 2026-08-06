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
```

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

`gen_dial.py` is retained for historical/experimental dial generation. The
current Dashboard draws its gauge with LVGL and does not consume that image.

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
