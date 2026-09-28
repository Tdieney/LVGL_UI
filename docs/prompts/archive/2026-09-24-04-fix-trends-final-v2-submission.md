> ARCHIVED — historical context only. Current handoff: [../CURRENT.md](../CURRENT.md).

# CURRENT — prove visible Trends feedback and close review gaps

Date: 2026-09-24
Status: **IMPLEMENTED — awaiting review**
Scope: ONE consolidated fix following the [independent review](../../reviews/2026-09-24-trends-final-independent/REVIEW.md). All 5 verification gates passed; delivery package in [docs/reviews/2026-09-24-trends-final-v2/REPORT.md](../../reviews/2026-09-24-trends-final-v2/REPORT.md). The owner's accepted Quiet Architecture appearance is frozen.

## Read and preserve

Read AGENTS.md, docs/prompts/README.md, docs/README.md, docs/HARDWARE_OVERVIEW.md, docs/MCU_BASELINE.md, docs/UI_DESIGN_BRIEF.md, docs/user/SMART_HUB_UI_GUIDE.md, this prompt, the independent review and the submitted [delivery report](../../reviews/2026-09-24-trends-final-delivery/REPORT.md). Preserve the dirty worktree, 40/40 accepted behavior, history semantics, Auto rules, wire contract and visual assets. No commit, push, reset, clean or stash. Do not change UI appearance, text sizes, radio drivers, Node firmware, packet schema, Flash persistence or MCU draw-buffer architecture. LCD is 800x480 RGB565 with one 800x10 partial buffer; visible text >=18px.

## P1 — prove first visible feedback on actual LVGL schedule

CHECK39 passes with Stage 1 **0 flushes / 0 pixels** in both default and 40 KiB runs. The separate benchmark forces `lv_refr_now`, unlike the normal `ui_tick()` -> `lv_timer_handler()` loop. Fix scheduling so a tap's selected-card feedback is visibly flushed before heavy chart work in actual application integration, including with a realistically delayed asynchronous flush. Do not call `lv_refr_now` from touch/ISR or while flush is pending; do not add an arbitrary delay. A small explicit stage/state machine or post-flush-ready scheduling is acceptable if memory- and port-safe. Specify the required order/contract for owner's firmware loop. If physical LCD first-feedback cannot be established without target integration, label it UNVERIFIED and supply exact measurement hooks.

Create a durable integration test with a realistic LVGL input event, timer progression, UI tick order and delayed flush-ready callback. Assert **positive** Stage-1 card pixels/calls (not only an upper bound), selected card visibly matches tap while chart is pending, final chart/stats/title all match latest tap, and no invalid flush-ready/reentrant LVGL call. Test one tap, rapid three-tap burst, new telemetry before completion, link loss/recovery, tooltip, page exit/re-entry and same-metric no-op. Measure tap-to-first-flush and tap-to-final-flush under a declared simulated QSPI bandwidth/latency; do not call that real touch-to-photon. Keep idle zero flush, 100-tap no heap drift and accepted rendering bounds. Existing standalone benchmark is insufficient alone.

Stage 1 currently changes Trends title before Stage 2 changes plot. Ensure no visible new title is attached to old plot/axis/stats. Preserve geometry/style; either commit title with chart or mark the interim chart unambiguously pending without unnecessary copy. Add intermediate-frame assertion/capture. Measure single-switch cost separately from burst coalescing; report that ~46% saving applies to bursts only. Do not claim <5 ms from coarse 0.00-ms host clock readings or immediate flush port.

## P2 — finish Windows verifier safety (host tooling only)

CIM enumeration exceptions are swallowed by worker polling loops; fixture 6 tests only an exited process. Propagate discovery failure to nonzero phase result with safe cleanup, or document a provably safe alternative; never report PASS after losing descendant visibility. Remove/restrict `Terminate-OwnedProcessTree` raw-root-PID path when `ExpectedStartTime` is absent; terminate captured task-owned identities only. Inject denied enumeration, denied kill, PID reuse and orphan scenarios via controlled mocks/owned workers; verify no unrelated kill or false PASS, then rerun full verifier in a permitted environment. This is Windows test-tool scope, not an MCU PID feature.

## P2 — close the specific E2 and evidence gaps

Preserve existing 40 checks. Extend CHECK40 to assert combined CO2/VOC faults, minute gaps and partial recovery across all four Trends metrics' Current/Min/Max/chart (including unaffected Temp/Humidity retention), plus Fan, Purifier and Humidifier Auto editors. Verify valid VOC=0, three fresh frames after recovery, hold/active_low and command echo/readback. Make assertions genuinely table-driven if reported as such; otherwise call sequence procedural. Avoid brittle incidental label matches such as a generic visible "0" without scoping to intended editor/value.

Keep useful allocation-wrapper/transient measurements, but reconcile docs/MCU_BASELINE.md and delivery report: actual draw buffer is 16,000 B; match 40 KiB minimum/drift to latest logs and state which instrumentation is host-only. Confirm source/config/binary identity for final 42/40 KiB runs and add actual-loop before/after visual comparison for unchanged accepted screens. Do not declare 1,896 B transient 40 KiB host margin safe on owner's ~99.9%-used MCU without target ELF/map, LVGL config, task stacks, DMA and radio buffers. Hardware timings remain UNVERIFIED until measured on board.

## Final verification and delivery

After LAST edit run `python tools/generate_assets.py --check`, `tools/build_sim.bat`, `tools/run_regression.bat`, `tools/export_mcu.bat --verify`, matched 42/40 KiB stress and safely repaired `tools/verify_production.bat`. Save fixture source, raw logs/exit codes, hashes, stage-specific flush/timing trace and native 800x480 captures/pixel comparison in ONE new `docs/reviews/<delivery>/` directory. Self-review full diff and affected end-to-end flow, fixing in-scope regressions before handoff. State skipped/failed gates and target-only limitations; no zero-bug/all-complete claim. Update PLAN.md, DEV_LOG.md, docs/README.md, docs/prompts/README.md, docs/MCU_BASELINE.md and this SAME CURRENT to IMPLEMENTED — awaiting review only after delivery. No new staged prompt.
