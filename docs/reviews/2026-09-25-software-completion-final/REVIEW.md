# Final independent review — Smart Hub UI software

Date: 2026-09-25
Disposition: **ACCEPTED for host-verified software integration**. This is not physical-target sign-off or a promise of zero defects. The owner-accepted Quiet Architecture pixels remain unchanged.

## What was reviewed

- Complete F1–F4 and N1–N2 delivery, affected `sim_pc/main.c`, `tools/run_regression.ps1`, `tools/verify_production.ps1`, the current handoff and report, native captures, RAM/performance evidence, MCU export boundary, and `git diff --check` (pass).
- N1 now reads the simulator process result directly and requires exit code **1** plus the exact invalid-directory diagnostic. Direct `sim_pc/build/sim_pc.exe --shots NUL` independently exited 1. The positive generator produces an exact 28-file manifest of 800×480 RGB565 frames (768,000 B each); missing/incorrect frames fail the regression gate.
- CHECK 39 no longer silently skips its required hooks and labels. The two-stage interim native screenshot shows VOC selected while CO₂ title, plot and stats remain coherent. Of 28 submitted screenshots, 27 hashes match the previous accepted native captures and the one new interim capture was inspected visually.
- N2 production verifier fails closed on simulated live CIM denial, skips failed rediscovery during cleanup, and protects unrelated processes by PID plus StartTime. Fixture 6c proves termination of the verified root and manually registered known process. That process is separately launched by the fixture, not an actual OS child of the root; thus the fixture does **not** prove discovery or cleanup of unobserved descendants under CIM denial. The delivery report now discloses this boundary. Normal child discovery and timeout cleanup are exercised in separate fixtures. This distinction is a limitation, not a reproduced UI defect or a reason for another broad fix round.

## Independent checks on the current worktree

| Check | Result |
|---|---|
| `tools/run_regression.bat` (includes build, 40 checks, N1, 28-shot manifest) | Exit 0; 40/40 |
| `build-40k/sim_pc.exe --regression` | Exit 0; 40/40 |
| `python tools/generate_assets.py --check` | Exit 0 |
| `tools/export_mcu.bat --verify` | Exit 0 |
| `tools/verify_production.bat` | Exit 0; all four phases and process fixtures pass (managed-sandbox escalation was required for CIM access) |
| Direct `sim_pc/build/sim_pc.exe --shots NUL` | Exit 1 as required |
| Standalone benchmark fixture | Exit 0; Stage 1 5 flushes / 34,532 px; Stage 2 18 flushes / 105,843 px; 100-switch heap drift 0 B |
| `git diff --check` for affected tracked files | Exit 0 |

The simulator build emits one upstream LVGL CMake CMP0177 **developer policy warning**; no C compiler warning was reported under the isolated `-Wall -Wextra -Werror` build. The 3.45/10.58 ms display-bus figures assume a nominal 20 MB/s transfer rate and are not measured LCD touch-to-photon latency.

## Remaining target-only acceptance

The owner still needs the final MCU ELF/map and `lv_conf.h`, RTOS stack/DMA/radio allocations, and measurements on the actual LCD/touch/LoRa hardware. The 42 KiB default and 40 KiB opt-in profiles are host-tested starting points, not a guaranteed fit for the reported ~99.9%-used target RAM. No further software correction prompt is opened on this review.
