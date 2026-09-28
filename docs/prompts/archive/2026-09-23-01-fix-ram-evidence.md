> ARCHIVED — historical context only. Active handoff: [CURRENT](../CURRENT.md).
> Replaced after owner reported live Trends and Auto failures. Open E1/E2 evidence items are carried into the new single handoff.

# CURRENT — close RAM evidence and regression coverage

Date: 2026-09-21
Status: **READY FOR HANDOFF**
Scope: ONE bounded verification/documentation delivery, not another UI redesign.
Owner requests independent review, preserved accepted appearance, and trustworthy
RAM/performance optimization. The technical lead specifies remaining proof below.

## Read first

Read AGENTS.md, docs/prompts/README.md, docs/README.md,
docs/HARDWARE_OVERVIEW.md, docs/MCU_BASELINE.md and docs/UI_DESIGN_BRIEF.md.
Read [latest independent review](../../reviews/2026-09-21-g1-g4-independent/REVIEW.md)
and its logs/probes/source identity. Earlier prompts are historical, not extra
scope. Use ui-ux-pro-max only for relevant interaction/readability checks.

## Decision and boundaries

G1 link layout and G2 const-style/config fixes are ACCEPTED. No new production
UI bug reproduced. All34 regression checks, export, isolated production tests
and full assert-enabled production smoke pass. Do not reopen these fixes.

Keep current accepted800x480 RGB565 appearance, assets, colors, typography>=18px.
No icon generation, layout redesign, font/history cuts, packet changes, radio/
RS485 drivers, Flash persistence, Node automation, or new Auto policy.
Do not commit/push/reset/clean/stash; preserve dirty worktree and old evidence.

Primary scope: simulator-only verification, diagnostics and honest documentation.
Do NOT alter production UI to make missing tests easier. A production fix is
in scope only if your new test reproduces a real defect; document it and verify
all affected flows in this SAME delivery. Do not split into phases/handoffs.

## E1 — measured RAM proof and corrected accounting

Default42KiB pool43008B + one10line buffer16000B =59008B COMPONENT subtotal,
not wholeUI.40KiB candidate pool40960+buffer16000=56960B; fixed saving2048B.
Default theme object text2944/data0/bss0; 544B writable reduction vs last review.
Do not add free arena headroom again as a reduction in linked SRAM.

Fresh reviewer measurements:
-42KiB CHECK8Home free7144/largest5344; CHECK31minsettled7592/largest6104,drift0.
-40KiB CHECK8Home free5080/largest3280; CHECK31minsettled5480/largest4000,drift+72.
-Both34/34PASS. CHECK31 drift limit is256B; CHECK9's separate limit is64B.
-Production host x64 ui.c with hooks disabled: data576+bss4064=4640B
 (assert-enabled smoke object). '~250B additional UI state' is not sufficient.
-Default and existing root40K caches use different compilers; log toolchain and
 config explicitly. Use the same compiler/config except heap size for a fair
 candidate comparison; never relabel host results as target results.

Finish previously requested transient/largest-allocation verification:
1. Add reproducible HOST-ONLY allocation diagnostics using test-only hooks or
   an isolated instrumented LVGL copy/build. Do not change exported LVGL/source
   config or allocate a diagnostic framebuffer on the MCU.
2. Measure live free and largest block at allocation/render construction
   boundaries, not just after ui_tick/advance_ui. Account for realloc's old/new
   overlap and rendering scratch/cache allocations. Monitor must not allocate
   from the pool being measured. Pinned lv_mem_monitor.max_used alone has
   incomplete realloc accounting; do not use it as the sole peak proof.
3. Record max requested allocation, failed nonzero allocation/reallocation
   count, minimum live free/largest block, owning scenario and margin for
   contemporaneous requests. A global max request from boot is not necessarily
   required in every later state: show timing/context rather than blindly
   comparing unrelated extrema.
4. Run42KiB and40KiB through splash, all pages, all3 Auto editors, dropdown,
   validation, Save/Cancel, fault/recovery and existing50cycle stress.
   Keep all current thresholds unchanged (total free>4096B, drift checks).
   Verify the diagnostic failure path with a bounded host negative control;
   never suppress a real allocation failure or assert by disabling checks.
5. If40KiB fails stricter checks, retain42KiB and report the observed failure.
   Reporting a qualified rejection of the candidate completes this task;
   no requirement to force40KiB to pass or rewrite the UI.
6. Provide non-overlapping ledger from production-config object/map evidence:
   pool, drawbuffer, additional UI static state, LVGL/port/application state
   where available; read-only sections separate. Identify ABI/config and
   alignment. Mark target unknown instead of estimating a wholefirmware total.
7. Keep42KiB/10lines as defaults.8lines saves3200B arithmetically but is NOT
   approved without target DMA/transfer/timing evidence.

Correct reports/live baseline:
-CHECK8 values must come from CHECK8, not CHECK31cycle1.
-+72B passes256B, not64B.
-UI_HEX example must match actual .full source, not obsolete .ch pseudocode.
-Current ui_theme_init sets gradient cache0; remove stale live1024Bgradient
 claim without changing accepted colors.
-Reviewer paired captures have one offline Auto paused label-placement
 difference; don't claim every page body is100%identical without checking.
-Preserve author historical logs; add erratum/current measured report rather
 than silently rewriting old raw results. Update live summaries to precise status.

## E2 — complete durable fault/transition assertions

Current34i covers individual faults/offline/recovery for Home and relevant
Auto editors. It does not cover the requested Trends fault matrix or combined
faults/partial recovery. Reviewer's observational probe is useful but exit0
alone is not a regression assertion.

Extend the standard suite with a compact table-driven matrix:
-Each individual validbit,co2_timeout,th_timeout,rs485_fault,offline; selected
 meaningful combinations (both bus timeouts, timeout+other invalidbit,
 globalfault+individualinvalid) and partial then full recovery.
-For every fixture assert actual Home values, all4 Trends Current/selector
 states, and all3 Auto editors including unaffected live readings.
-Keep validVOC0. A temperature-only invalidbit must not invalidate validRH;
 th_timeout masks both. CO2 timeout masksCO2/VOC; globalbusfault/offline all.
-Seed history first. Assert history retained through loss; unavailable samples
 do not become fakevalid chart readings; recovery restores live values without
 erasing previous valid history. Preserve Auto inhibition on unavailable input.
-Add explicit same-number metric/unit transitions, unavailable/empty/recovery
 and chart rebuild to existing accepted numeric/unit tests.
-Existing real no-op Save callback/hold checks are accepted. Extend their
 imported custom-name fixture to23bytes+NUL and compare the complete semantic
 config before/after; do not memcmp uninitialized padding. Preserve0callbacks,
 hold/qualification and unchanged config on no-op; changed threshold/preset
 commits once; Cancel discards. Do not change name policy.
-Each test must return nonzero on failure and run via standard regression.
 Maintain existing marker pixels/no-bridge, zero-redraw12fixtures,
 same-tab/dropdown stress, pointer Retry and error-latch checks.
-Add bounded negative controls for new helpers where practical; no deliberately
 broken code left in production, no manufactured successful output.

No new missing-data wording: Temp/RH dash/nofooter; Home/Devices noUnknown,
single unavailable indicator, Retry kept. Trends Data unavailable / No readings
yet, hero copy, modes/Auto paused and Auto No live data remain unchanged.

## Frozen architecture and Auto behavior

LVGL8.4; one partialdrawbuffer; static caption lifetime/fixed dynamic buffers;
no per-tick allocation, float/trig hotpaths or ISR LVGL.
UI owns six LoRa globals exactlyonce in ui.c; command8B/status20B.
Hub owns polarity; Node applies rawGPIO/readback. UI/application serialized;
startup/reconnect ui_is_tx_ready gating preserved.
FanCO2 ON>=1000/OFF<=800,bounds0..10000,step50,gap50.
PurifierVOC ON>=150/OFF<=100,threshold1..500,step5,gap5; validtelemetry0.
Humidifier ON<=40/OFF>=50,bounds0..100,step1,inversegap5.
Three fresh qualifying frames after10shold. No action offline/invalid/pending/
latched; no forcedOFFonloss, no timedManual→Auto. Explicit Retry/Manual→Auto
rearm; reconnect/lateACK/thresholdedits do not clear latch. Imported nonstep
thresholds remain valid, Cancel discards, AutoBack retainsdraft.

## Final gates and deliverables

After LAST relevant edit run:
- python tools/generate_assets.py --check
- tools/build_sim.bat
- tools/run_regression.bat
- tools/export_mcu.bat --verify
- tools/verify_production.bat
- matched42/40KiB diagnostic builds and regressions.
If production style/config changed despite accepted baseline, rerun full
consistent assert-enabled production smoke and RGB565swap probes too.

ONE new docs/reviews/<delivery>/ folder with E1/E2 disposition, source/config/
binary hashes, real logs, reproducible commands, allocation statistics and
measured ledger. Self-review full finaldiff and fix any introduced regression
before handing back; do not send owner intermediate issue groups.
Update docs indexes, MCU_BASELINE, PLAN, DEV_LOG and SAME CURRENT status
IMPLEMENTED — awaiting review. Do not scatter new prompts.

External target limitation: actual MCU map/ELF/linker script/lv_conf, SRAM banks,
display buffers, stacks/runtime watermarks and QSPI timing are not supplied.
If available, inspect them; otherwise list once as NOT TESTED and report host
closure separately. Do not claim99.9%RAM solved or block safe host work waiting
for missing hardware. A documented rejected40KiB candidate plus corrected tests/
ledger is a valid completed delivery; target integration is a separate sign-off.
