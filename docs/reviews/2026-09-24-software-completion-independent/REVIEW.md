# Independent technical-lead review — 2026-09-24 software-completion submission

Status: **one consolidated correction requested**. The UI's accepted native appearance is retained; no new confirmed runtime UI defect was found in this review. Do not interpret the host green gates as target-hardware sign-off.

## Independently verified

- `python tools/generate_assets.py --check`, `tools/build_sim.bat`, `tools/run_regression.bat` (40/40), 40 KiB `sim_pc.exe --regression` (40/40), `tools/export_mcu.bat --verify`, and `tools/verify_production.bat` all completed successfully. The production verifier required an elevated retry because the managed sandbox denied CIM access; this is an environment restriction, not proof of a product defect.
- The native `trends_stage1_interim.png` visibly shows VOC selected while CO₂ title/chart/stats remain; representative Home, Devices and Auto captures preserve the accepted look. The benchmark executable reproduces Stage 1 = 5 flushes / 34,532 px, Stage 2 = 18 flushes / 105,843 px, 100-switch heap drift = 0 B. These are host measurements, not physical LCD latency.
- The 28 submitted PNG captures exist. No changes to production UI pixels were required from this review.

## Consolidated findings

1. **F1 / P1 — `--shots` is fail-open and the main regression runner does not validate the required image set.** Reproduced `sim_pc/build/sim_pc.exe --shots NUL`: every framebuffer write failed, yet the process printed success and returned 0. `sim_pc/main.c:8019–8409` ignores `save_framebuffer()` failures; the new interim assertion only prints `ERROR`, and missing hooks skip capture. The final message says 24 although 28 captures are submitted. `tools/run_regression.ps1:74–80` only requires at least 10 raw files, so a missing interim capture can be green.
2. **F2 / P1 — CHECK 39 can silently skip its key assertions.** `sim_pc/main.c:7095+` guards the Stage-1/Stage-2 and async-flush sections with optional hook checks, and title/unit assertions are optional when labels vanish. This undercuts the gate claimed to prove the lag fix. Require fail-closed prerequisites in both memory profiles.
3. **F3 / P2 — live CIM-denial cleanup is not proved and can re-enter failing discovery.** `tools/verify_production.ps1:271–288` catches enumeration errors and invokes `Terminate-OwnedProcessTree`, which calls `Get-VerifiedProcessTree` again. Fixture 6 tests missing start time and an exited process, not a live CIM access denial. This is a verifier safety/evidence issue, not a confirmed MCU UI defect. Test controlled owned-process denial and ensure failure remains explicit without unrelated kills or silently orphaned known workers.
4. **F4 / P2 — report overstates evidence.** `build_sim.log` has an LVGL CMake CMP0177 developer warning despite the report/PLAN saying zero warnings. “100% complete/fully verified” is incompatible with the report's own target-hardware UNVERIFIED section and the above fail-open tests. The benchmark fixture's build comment points to the older v2 path, not its delivered location. Correct reproducibility statements and leave raw logs intact.

The sole corrective handoff is [CURRENT](../../prompts/CURRENT.md). Gemini should complete and test all four items before the next technical-lead review; no piecemeal rounds or visual redesign. Physical MCU RAM map/stack/DMA/radio reservations, LoRa behavior, and LCD touch-to-photon remain owner/target-only checks.
