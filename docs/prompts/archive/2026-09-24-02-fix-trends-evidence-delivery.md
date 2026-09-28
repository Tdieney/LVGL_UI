# ARCHIVED — historical context only

Superseded by [CURRENT](../CURRENT.md). Retained as the 2026-09-24 submitted Trends responsiveness handoff.

# CURRENT — actually improve Trends responsiveness and close review defects

Date: 2026-09-24
Status: **IMPLEMENTED — awaiting review**
Delivery Report: [2026-09-24 Consolidated Delivery](../../reviews/2026-09-24-trends-responsiveness-delivery/REPORT.md)
Scope: ONE consolidated fix, independent-quality verification and honest hardware handoff. Owner reports lag switching CO₂/VOC/Temperature/Humidity in Trends. The 2026-09-23 developer delivery fixed several history defects but did not reduce measured display work; see [2026-09-24 independent review](../../reviews/2026-09-24-trends-responsiveness-independent/REVIEW.md). Optimization tactics below are technical-lead proposals, not new owner-approved product or visual changes.

## Prerequisites and boundaries

Read AGENTS.md, docs/prompts/README.md, docs/README.md, docs/HARDWARE_OVERVIEW.md, docs/MCU_BASELINE.md, docs/UI_DESIGN_BRIEF.md, docs/user/SMART_HUB_UI_GUIDE.md, this prompt and the independent review. Preserve the dirty worktree and current accepted Quiet Architecture appearance; do not replay historical redesigns. Do not commit, push, reset, clean or stash. The archived prior handoff is history only.

LVGL 8.4, 800×480 RGB565, QSPI LCD with GRAM, one 800×10 draw buffer, visible fonts ≥18px. Keep current 16×4 bounded session history, Auto semantics, six direct LoRa globals, 8B/20B wire contract, active_low mapping and Node verbatim readback. No full framebuffer, second draw buffer, unmeasured RAM cache, periodic allocation, Flash persistence driver, RF/RS485 driver, packet-schema change, new icon/palette/geometry, or LVGL UI work in an ISR. UI memory is near target capacity; default 42 KiB LVGL pool and 10-line buffer remain unless a complete target map proves a change safe.

## P1 — make the reported lag measurably better

Independent before-and-after measurement is *identical*: each populated metric switch remains 151,615–160,984 pixels, 303,230–321,968 RGB565 bytes, 25–27 flushes; 100 switches remain 31,313,358 bytes/2,623 flushes. Same-selection zero redraw and zero settled heap drift pass, but those do not establish responsiveness. CHECK39 prints volume and asserts only heap/no-op/idle. Pinned LVGL's lv_chart_set_point_count already exits when count is unchanged, so that new caller guard is not a bus optimization. The old and new host wall times are not MCU timing.

Start with a reproducible, source/config-matched benchmark for empty, singleton, fully populated, invalid-gap and live-updating Trends. Record per-switch actual rectangle coordinates/area, total pixels/bytes/calls, render CPU time, touch→selected-card feedback→finished chart time, peak/free/largest heap and repeated 100-tap behavior. Assert a concrete regression budget in CI after measuring an attainable improved state; do not simply print totals. New reviewer probe [metrics_probe.c](../../reviews/2026-09-24-trends-responsiveness-independent/metrics_probe.c) shows individual flush width max 466 px and **zero 800px-wide flushes**. Do not optimize under the false assumption of unavoidable full-width slices. LVGL single-buffer refresh waits for flush-ready before reusing the buffer: do not claim DMA transfer overlaps drawing the next slice without a demonstrated independent staging buffer/port behavior.

Reduce actual invalidation/bytes or measured target latency, not just function calls. Preserve exact appearance, chart data/gap/tooltip semantics and immediate selected-state clarity. Explore tight invalidation bounds for selectors/chart/stats, defer or coalesce rapidly superseded metric requests (latest tap wins), and stage selected-card feedback before heavy chart work in application context if this measurably helps. Do not call lv_refr_now from touch/ISR with an in-flight asynchronous flush; no stale or mismatched metric/chart state on page exit, reconnect or new readings. Optimize alpha polygon rasterization only with CPU evidence. Do not increase draw buffer to 20 lines or add another buffer based on the developer's speculative report: +16 KiB RAM needs a real memory map and transient headroom first.

The repository lacks the owner's target display port and firmware. Provide precise safe instrumentation at touch dispatch, render start/end, flush start/ready, QSPI bytes/calls and radio/UI scheduling; request owner target traces for hardware sign-off. If no target is available, improve and prove host display work, label hardware latency **UNVERIFIED**, and do not claim the lag fixed. Acceptance is a material before/after reduction or directly measured target touch→feedback/plot improvement; document the chosen numeric budget, full trace and visual equivalence. No merely cosmetic change or unverifiable “instantaneous at 40–80 MHz” claim.

## P1 — repair production verifier ownership safety

tools/verify_production.ps1 gathers descendants by numeric root PID before verifying the root StartTime, accepts tracked descendant PIDs without identity checks, then .Kill()/taskkill-s each numeric PID even in a finally block after normal worker completion. PID reuse can target unrelated user processes. Do not simply catch and ignore access denied. Identify each task-owned process by a live process handle/start time (and verified parent lineage when applicable) before termination; never enumerate/kill descendants of an unverified recycled root. Fail with a clear diagnostic if safe cleanup cannot be established. Add non-destructive fixture tests for normal completion, timeout, access denied, recycled root/descendant and no unrelated process termination. Only rerun the full verifier after this is safe; retain its collision/negative controls.

## P2 — truthful session time at counter rollover

The 24h/100h defect is fixed. But autonomous history stores a uint16 minute; at session minute 65,536 the chart shows “Last 0 min” and its axis goes from +1092h15m back to +0m. Choose a bounded solution that remains truthful after 45.5 days, or explicitly implement/document a supported session limit with a safe UI fallback. Measure RAM impact if timestamp representation changes. Add an end-to-end regression crossing 65,535→65,536 and preserve external wall-clock modulo/midnight behavior, 16-slot gap continuity and sample validity. Update misleading minute_of_day field documentation if it stores elapsed-session minutes too.

## Carried E1/E2 — close in this SAME delivery

E1 is still open: honest non-overlapping whole-UI/firmware RAM ledger; matched default42KiB and opt-in40KiB host transient allocation/render diagnostics including largest request, smallest contemporaneous free/largest block, failures and 50-cycle stability; correct stale CHECK8/CHECK31, 256B/64B, UI_HEX and gradient-cache wording. Keep 42KiB/10-line default unless target ELF/map, lv_conf, stack, DMA and firmware occupancy justify otherwise. Target “99.9% RAM solved” is not supported by pool-free percentages.

E2 is still open: durable combined sensor-fault/partial-recovery matrix across Home, Trends and all three Auto editors, independent unaffected metrics/history retention, and full 23-byte name no-op Save/config/hold invariants. Existing CHECK34/35/36–39 should be extended, not weakened. Verify the complete RX→ui_tick→command→echo/readback Auto flow, including boot/reconnect, manual override, Retry, timeout and active_low; no Node-side automation.

## Verification and one delivery

After the LAST relevant edit run: python tools/generate_assets.py --check; tools/build_sim.bat; tools/run_regression.bat; tools/export_mcu.bat --verify; safely repaired tools/verify_production.bat; matched 42/40KiB diagnostics; fresh performance and visual comparisons. Save all command outputs/exit codes, exact source/config/binary hashes, benchmark tables, transient RAM evidence, native 800×480 captures for four metrics/empty/gap/Auto states, and target limitations in ONE new docs/reviews/<delivery>/ report. Do not call a 39/39 suite proof of LCD smoothness. Correct docs/user/SMART_HUB_UI_GUIDE.md checklist items that still require external ui_feed_history for basic Trends and Flash persistence for this demo; align docs/MCU_BASELINE.md and docs/prompts/README.md with actual status. Update PLAN.md, DEV_LOG.md and this SAME CURRENT to IMPLEMENTED — awaiting review only after self-reviewing the final diff and fixing in-scope regressions. Report any unavailable target evidence as such; no staged prompts or silent skipped checks.

