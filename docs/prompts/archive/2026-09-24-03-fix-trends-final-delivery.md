# ARCHIVED — historical context only

Superseded by [CURRENT](../CURRENT.md). This is the developer's 2026-09-24 final-delivery handoff; acceptance claims remain subject to independent review.

# CURRENT — close actual Trends lag and remaining proof gaps

Date: 2026-09-24  
Status: **IMPLEMENTED — awaiting review**  
Delivery Report: [2026-09-24 Trends Final Delivery](../../reviews/2026-09-24-trends-final-delivery/REPORT.md)  
Scope: ONE final consolidated improvement and evidence delivery following the [independent review](../../reviews/2026-09-24-trends-delivery-independent/REVIEW.md). Owner reports lag on real LCD when switching Trends metrics. Owner accepted the current Quiet Architecture visuals; do not redesign them. Technical-lead tactics below are proposals, not new owner-approved product behavior.


## Read and preserve

Read AGENTS.md, docs/prompts/README.md, docs/README.md, docs/HARDWARE_OVERVIEW.md, docs/MCU_BASELINE.md, docs/UI_DESIGN_BRIEF.md, docs/user/SMART_HUB_UI_GUIDE.md, this prompt and the independent review. Preserve dirty worktree, accepted visuals, working 100-minute gap/external-ownership/65,536-minute timestamp fixes, Auto rules, tests and assets. No commit, push, reset, clean or stash. Archived prompts are history only.

LVGL 8.4, 800×480 RGB565, one 800×10 partial buffer, LCD GRAM, text ≥18px; no additional framebuffer, second draw buffer or 20-line expansion without a real target memory map. Static captions, fixed dynamic buffers, bounded hot-path CPU/heap. Six LoRa globals remain defined once in ui.c; 8B/20B wire contract, Hub active_low mapping and Node verbatim GPIO/readback unchanged. No radio/RS485 driver, Flash persistence driver, multi-node architecture or ISR LVGL.

## P1 — measure and improve perceived metric-switch latency, not just counters

Current improvement is real but **only 3.0–3.2%**: 146,799–156,168 px / 24–26 flushes / 293,598–312,336 B per populated switch; 100 switches still 30,359,790 B. The developer's CHECK39 guards these values but does not measure tap→selected feedback or chart completion. The current handler still synchronously updates both selection and chart on every tap. No real LCD trace exists, so do not declare the owner's lag solved.

Use the existing [metrics probe](../../reviews/2026-09-24-trends-responsiveness-independent/metrics_probe.c) as a reproducible host baseline, and save a durable benchmark fixture plus raw before/after results under the new delivery folder (not ignored scratch/). Profile empty, singleton, full, gapped and live-updating histories, all four metric directions, rapid taps, selected state, tooltip and page exit. Measure actual flush rectangles/pixels/calls, host render time and LVGL pool/transient use. Separately supply a target-side instrumentation plan for touch event→first visible selected feedback→finished chart, CPU render and flush-ready/QSPI DMA; if the owner's firmware is unavailable, clearly mark these target timings UNVERIFIED.

Pursue a **material** improvement in either measured display traffic or target perceived response. First diagnose the remaining invalidation union; remove only verified redundant updates. If single-switch traffic is fundamentally chart-sized, implement safe latest-request coalescing and a feedback-first render stage (as a concept from motor-control) in application context, then test with an artificially slow asynchronous flush callback on host: the chosen card must become visible promptly, the final chart/title/stats must agree with the latest tap, and no stale/intermediate metric is falsely labeled. Maintain correct behavior through repeated rapid taps, incoming sensor frames, disconnect/reconnect, tooltip inspection and leaving/re-entering Trends. No lv_refr_now in touch/ISR while flush is pending, no data suppression, no changed Quiet Architecture appearance. Quantify how much the chosen approach improves first feedback and completion; do not simply move time elsewhere or weaken checks. If real target traces show a different bottleneck, fix that measured cause and document it.

Keep same-metric zero redraw, idle zero redraw, no 100-switch heap drift and existing pixel budgets. Add a regression for the *new* measured response property (two-stage/last-tap semantics or a justified stricter traffic budget). The target hardware sign-off still requires owner measurements; do not invent an FPS or “instantaneous” DMA result.

## P1 — finish process cleanup safely

tools/verify_production.ps1 still contains a KnownDescendantPids fallback that accepts bare numeric PIDs and snapshots their StartTime only at cleanup. Remove or reject this path; cleanup may act only on task-owned process identity captured while a verified parent is alive. Get-VerifiedProcessTree currently swallows CIM enumeration errors and returns an empty tree; that can leave children alive while a phase appears to pass. Revalidate parent identity while traversing, fail closed with a clear error when descendants cannot be enumerated, and never terminate unrelated processes. Tests must inject denial/reuse through mocks or owned test processes; do not use System PID 4 or explorer.exe as a live negative target. Verify clean completion, timeout with child, reused root/child, denied enumeration/kill, and no orphan/false PASS. Then run the full verifier in an environment that permits required process inspection; if permissions forbid that here, report the gate as unverified, not PASS.

## E1 — complete only missing RAM proof

Default 42KiB and opt-in40KiB 39/39/50-cycle settled tests are accepted. The +256 B history BSS change and host component ledger are noted. Still capture **allocation-level largest request, failure count, and minimum free/largest block during actual allocation/render transients**; Check31 currently tracks minima independently at sampled states, so “largest block at peak free use” is not established. Record the free and largest block as a pair at the same instant and distinguish both from settled values. Provide matched source/config/binary hashes for 42/40KiB runs and an explicitly non-overlapping host ledger; do not call it the full target firmware RAM map. Reconcile the stale 2026-09-21 top section of docs/MCU_BASELINE.md with current figures, and state that target ELF/map, LVGL config, stacks, DMA and radio buffers are still needed before approving +256 B on the owner's 99.9%-used MCU. If target RAM has no room, propose a smaller truthful timestamp encoding and measure it.

## E2 — turn the claimed fault matrix into assertions

Keep existing CHECK30/34/35 and 23-byte name/no-op Save tests. Add a small table-driven sequence covering **combined** sensor faults and partial recovery across Home, selected Trends current/Min/Max/chart-gap retention for each metric, and all three Auto editors. Assert unaffected metrics remain live/history retained, valid VOC=0, correct three-fresh-frame/hold behavior and no command from unavailable inputs. Include an RX→ui_tick→command→echo/readback case across recovery and active_low. Do not count the current prose table or independent single-fault tests as the missing combined matrix.

## Verification and delivery

After the LAST source change: python tools/generate_assets.py --check, tools/build_sim.bat, tools/run_regression.bat, tools/export_mcu.bat --verify, 42/40KiB matched regressions/transient probe, and the safely repaired tools/verify_production.bat. Do not hide a skipped or hung gate. Save executable benchmark sources, raw logs/exit codes, source/config/binary hashes, RAM tables, native 800×480 captures for all four populated Trends metrics plus empty/gap/Auto states and same-state visual comparison under ONE new docs/reviews/<delivery>/ folder. Distinguish verified fixes from target-only limitations. Self-review the entire diff and affected end-to-end flow, fix in-scope regressions, then update PLAN.md, DEV_LOG.md, docs/README.md, docs/prompts/README.md, docs/MCU_BASELINE.md as needed and this SAME CURRENT to IMPLEMENTED — awaiting review. No staged prompts or all-complete claim without the evidence.

