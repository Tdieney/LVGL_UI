# ARCHIVED — historical context only

Superseded by [CURRENT](../CURRENT.md). Retained as the 2026-09-23 Trends responsiveness implementation handoff.

# CURRENT — make Trends responsive and close live-history edge cases

Date: 2026-09-23
Status: **IMPLEMENTED — awaiting review**
Scope: ONE consolidated implementation, performance measurement, regression and evidence delivery. Owner reports visible lag switching CO₂/VOC/Temperature/Humidity in Trends on hardware and asks for a smooth target UI. The accepted Quiet Architecture visual is frozen. The concrete optimization tactics below are technical-lead proposals, not new owner-approved visual or product changes.

## Read first and protect scope

Read AGENTS.md, docs/prompts/README.md, docs/README.md, docs/HARDWARE_OVERVIEW.md, docs/MCU_BASELINE.md, docs/UI_DESIGN_BRIEF.md, docs/user/SMART_HUB_UI_GUIDE.md, this prompt, the [independent review](../../reviews/2026-09-23-trends-auto-independent/REVIEW.md), and the [developer delivery](../../reviews/2026-09-23-trends-auto-delivery/REVIEW.md). Inspect current source/tests before edits. The archived prior prompt is historical, not authority. Preserve the dirty worktree and accepted assets/appearance. Do not commit, push, reset, clean, stash or edit the motor branch.

Implement and self-review the complete final diff and all affected end-to-end flows. Fix regressions inside this one delivery, not in a second staged prompt. The UI remains LVGL 8.4, 800×480 RGB565, one 800×10 partial buffer, display GRAM, visible text ≥18px; no motor-control screens/terminology/telemetry/assets. Six LoRa globals remain defined once in ui.c, Hub-side Auto and active_low mapping, Node verbatim GPIO/readback, 8B/20B wire contract, 16×4 bounded history and existing Save/Retry/error semantics. No radio/RS485 driver, Flash persistence, full framebuffer, second draw buffer, large RAM cache, unbounded heap use, or ISR LVGL.

## P1 — profile and reduce Trends switching latency

Baseline independent host measurement with 16 valid points per metric: a populated metric switch flushes **151,615–160,984 pixels / 303,230–321,968 RGB565 bytes / 25–27 callbacks**; 100 rapid switches flush 31,313,358 bytes; reselecting the same metric flushes zero. Probe: [perf_probe.c](../../reviews/2026-09-23-trends-auto-independent/perf_probe.c). This is *not* MCU latency. Current switch updates selector styles and chart synchronously; chart point-count/range/refresh, labels and 15 alpha-shaded segment polygons are plausible work. Do not declare CPU or bus the sole cause without measuring target timing.

Add a reproducible host benchmark to the normal test workflow for empty, singleton, full-valid, gapped and live-changing history; measure switch input→selected feedback→complete plot, pixel count, flush calls, largest dirty rectangle, 50–100 alternating/rapid taps, and settled idle/identical frame. Include before/after same-hash/config results and assert existing zero-redraw behavior. On the owner's target, if available, instrument touch event, UI update, LVGL render, flush start and flush-ready/DMA complete, total QSPI bytes/calls, and heartbeat responsiveness. Do not log from ISR or change wire scheduling. If the target project is absent, provide a small explicit integration checklist/measurement hook and mark target timing unverified; no fabricated MHz/MBps/FPS.

Optimize after the profile, preserving pixel-equivalent accepted appearance at 800×480 and all chart semantics. First remove redundant label/style/chart mutations and keep invalidation local to genuinely changed selector/plot/stats areas. Avoid repeated remove/add style allocation if state-based styling can reproduce the exact visual; preserve focus/touch behavior. Eliminate duplicate chart invalidation where LVGL already refreshes due to count/range/point changes. Do not suppress necessary redraw or leave stale pixels. Measure whether alpha polygons are CPU-expensive on target before changing their implementation; a cheaper fill must match the accepted plot visually. If switching still blocks touch while a large plot flushes, adapt the proven *concept* from ui/motor-control: coalesce rapid metric requests to the latest, give the selected card immediate feedback, and schedule the plot update in application context after that feedback has a chance to flush. Do not blindly copy its 90ms throttle or call lv_refr_now from touch/ISR when the target display port has asynchronous DMA or a pending flush. No intermediate wrong-metric chart should be presented as current. Account for re-entry, page exit, link changes and fresh sensor data during a pending switch.

Report objective before/after **pixels, callbacks, host wall time, heap free/largest/transient peak and target timings if available**. The goal is a material reduction in measured switch cost with unchanged visual output; if target measurements show the bottleneck is elsewhere, state evidence and fix the actual bottleneck. Keep RAM no worse than the validated default and no periodic heap churn. A screenshot-only or CPU-only claim is insufficient.

## P2 — correct three history time defects

1. Long outage: current ui.c caps an elapsed diff at 16 but inserts invalid minutes immediately after the old sample; recovery at session minute 100 after minute 0 gives slots 1…15 and 100, while the chart spaces them equally. Retain the newest contiguous 16-minute slot window, or an equivalent truthful mapping, with bounded O(16) work. Test 16,17,100-minute gaps, offline/recovery, wrap and unaffected metrics. Preserve no fake sensor values and no interpolation through invalid slots.
2. 24h uptime: autonomous samples store minute modulo 1440 and tooltip/axis call it elapsed time, so +24h becomes +0m. Keep the time semantics truthful for >24h sessions without unbounded memory. A bounded recent-window-relative label is acceptable only if visibly identified and consistent in axis/tooltip/span; otherwise store enough day/elapsed information. External ui_feed_history must remain wall-clock, and 23:59→00:00 external rollover must work.
3. Ownership change: ui_feed_history marks a metric external before an equal-sample no-op; if values/timestamps match the autonomous data exactly, revision does not change and axis remains relative (+10m) instead of external wall-clock (00:10). Ownership/time-base transition must invalidate only the affected metric even if the point bytes match. Refeeding the same external series again remains a no-op. Test selected and off-page metrics; no cross-metric reset.

Test-only probe source/results are in the independent review. Add durable regression assertions; do not rely on a developer prose claim.

## Carried open E1/E2 evidence — close in the SAME delivery

E1: Produce honest non-overlapping RAM ledger and matched 42KiB/opt-in40KiB host allocation/render transient diagnostics: largest request, smallest *contemporaneous* free and largest block, failures, 50-cycle stability. Correct historical CHECK8-versus-CHECK31, 256B-versus64B, UI_HEX example and stale gradient-cache wording without altering accepted visuals. Keep 42KiB/10-line default; 40KiB may be explicitly rejected by measured headroom. MCU 99.9% RAM is not solved without target ELF/map, full firmware lv_conf, stack/heap/DMA map and QSPI timings.

E2: Durable table-driven sensor availability and combined-fault/partial-recovery matrix across Home, Trends and all three Auto editors, including unaffected metrics and history retention. Full 23-byte name no-op Save/config/hold invariants. Fold new autonomous history/Auto integration and rapid-switch tests into the full suite. A test passing through simulator-only injected history is not sufficient for the live path.

The previous delivery did not supply matched E1/E2 artifacts. Do not remove these items merely because the current 35 checks pass.

## Verification, error paths and delivery

After the final source edit: python tools/generate_assets.py --check; tools/build_sim.bat; tools/run_regression.bat; tools/export_mcu.bat --verify; tools/verify_production.bat; matched 42/40KiB diagnostics and new switch benchmark. Keep old thresholds/assertions unless objectively justified and independently demonstrated. The independent production verifier rerun stopped in phase-one process cleanup at taskkill Access denied; investigate safely and report actual status. Identify verifier-owned processes before any termination; never kill unrelated user processes. Distinguish environment/permission failure from a UI defect. Include native 800×480 screenshots or pixel comparisons for all four populated charts, empty/singleton/gaps, selected-card feedback and Auto pending/error; no visual regression.

Deliver ONE new docs/reviews/<delivery>/ report with exact source/config/binary hashes, commands/exit codes/logs, performance and RAM tables, screenshot evidence, remaining limitations and target instrumentation instructions. Update docs/README.md, docs/MCU_BASELINE.md, docs/user/SMART_HUB_UI_GUIDE.md if behavior/integration changes, docs/UI_DESIGN_BRIEF.md only if a product-visible decision changes, PLAN.md, DEV_LOG.md, and this SAME CURRENT status to IMPLEMENTED — awaiting review. No claim of hardware smoothness, MCU memory approval or complete gate pass without executed evidence. Report genuine blockers together; no extra fix prompt or phases.

