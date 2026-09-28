> ARCHIVED — historical context only. Active handoff: [CURRENT](../CURRENT.md).
> Superseded 2026-09-21 after independent follow-up: R1–R7 repaired, R8 partial;
> owner requests simpler unavailable states; RAM/test delivery remains partial.

# CURRENT — preserve accepted UI; fix runtime defects and reduce RAM/render cost

Date: 2026-09-21. Status: **IMPLEMENTED — awaiting review; target RAM sign-off evidence pending**.
ONE consolidated implementation, self-review and final verification delivery.
All reproduced defects R1–R8 and unused-style heap reclamation (1,248 B)
implemented and verified: 34/34 checks PASS, production smoke test PASS.

## Owner authority and visual freeze

The owner has worked with the developer and ACCEPTS THE CURRENT NATIVE LOOK.
Use the submitted Sept21 native UI as the visual baseline, not older generated
concepts or prior corrective icon prompts. No redesign, regenerated icons,
palette/gradient changes, font shrink, spacing changes or removed features.
Preserve ordinary-state geometry and accepted icon masters exactly. Correct only
the demonstrated faulty states below; optimize internals invisibly.

Owner asks for a careful bug/performance/RAM review and a developer handoff.
Reported MCU RAM is99.9%; exact MCU/linker/config evidence is not yet supplied.
This prompt is technical-lead guidance, not authorization to invent firmware
facts, new hardware drivers, persistence, protocol or product requirements.

Read AGENTS.md, docs/prompts/README.md, docs/README.md, docs/HARDWARE_OVERVIEW.md,
docs/MCU_BASELINE.md and latest docs/UI_DESIGN_BRIEF.md; newer owner acceptance
overrides historical visual-reconstruction instructions.
Read [independent review](../../reviews/2026-09-21-runtime-ram-review/REVIEW.md),
[probe source](../../reviews/2026-09-21-runtime-ram-review/probe.c),
[probe results](../../reviews/2026-09-21-runtime-ram-review/PROBE.txt),
[34/34 regression output](../../reviews/2026-09-21-runtime-ram-review/REGRESSION.txt)
and [source identity](../../reviews/2026-09-21-runtime-ram-review/SOURCE_IDENTITY.json).
Inspect the fresh native captures there. Use ui-ux-pro-max for relevant interaction
checks only; generic web/vector recommendations do not override accepted LVGL art.

Preserve the dirty worktree and owner ZIPs/evidence. No reset/clean/stash, commits,
pushes or overwritten previous evidence. All changes must be in one final diff.
The issues below are traceability IDs, NOT staged handoffs or separate phases.

## Fix all reproduced defects in the same delivery

### R1 — unbounded duplicate style attachments (first priority)

ui.c:update_trends_tabs / ui_select_trend_metric repeatedly append a desired
style already attached. Actual20 same-metric selections consume~1.3KiB extra
heap, card style_cnt4→24. A real metric change can also update styles twice.

ui.c:on_dialog_preset_dropdown_event attaches five list styles and one label
style at every LV_EVENT_READY. LVGL8.4 close only HIDES the list;8 additional
opens inside the same editor consume776B, list style_cnt16→56.
Pinned lv_obj_t.style_cnt is6bits: this is an overflow/corruption risk.

Make attachment idempotent per object + style + selector. Prefer once-at-creation
or genuine state-change styling; no remove/re-add churn on identical state.
Do not silently remove default widget styles/pressed/checked behavior.
Already-selected metric click should not allocate/repaint unnecessarily.
Check all repeated style attachment paths, not just these two named lines.

Add >=200 same-selected metric taps and >=100 dropdown open/close cycles within
ONE persistent page/editor, including actual pointer interactions. Warm up caches,
record style counts, free/largest/fragmentation and verify no progressive growth,
no6bit overflow, no crash. Do not rebuild the parent between iterations.
Also retain existing mixed-metric/navigation/editor stress tests.

### R2 — one text owner per Trends value label

Current/Min/Max are each written first using legacy number+unit buffers, then
numeric-only buffers. CO2=100 -> RH=100 leaves `100%` plus another separate
`%`, because the numeric cache does not change.
Remove competing label writers. Keep formatted test/API buffers if needed, but
do not bind them to numeric UI labels. A cache optimization must also handle
newly created widgets and label backing-pointer ownership correctly.

Test all ordered metric pairs with equal numeric representations and different
units, empty/invalid transitions, unchanged Min/Max, and leave/re-enter.
Assert lv_label_get_text on actual visible widgets, not only cached test buffers;
use pixel/glyph checks to catch doubled/overlapping units.

### R3 — initialize chart range per new chart instance

Cached s_last_trends_range_min/max survive deletion. Rebuilt chart defaults to
0..100 while code skips setting its unchanged cached96..104 range.
Invalidate instance-local caches or explicitly initialize every new chart's range.
Assert actual LVGL chart min/max plus axes/pixels before and after same-history
Trends->Home->Trends; cover flat, nonflat, negative temperature and empty histories.

### R4 — retain every isolated valid reading

on_chart_draw_part_begin makes non-highlight points transparent, then manually
draws isolated points with that transparent descriptor.
[400,invalid,450,invalid,500] currently shows only the final dot.
Draw isolated valid markers visibly, preserving current last/inspected emphasis
and continuous-line appearance; never bridge invalid gaps.
Test multiple isolated points including first/interior/last, singleton and all
invalid; assert actual marker pixels, not just valid values in the series array.

### R5 — one coherent live-availability policy

Home, Trends and Auto live reading ignore fault flags while Auto evaluation
honors them. valid bits1 + rs485_fault1 currently still shows air Good and numbers.
Introduce a small read-only shared availability helper or equivalent that applies:
- offline -> no live readings;
- RS485 bus fault -> relevant bus sensors unavailable;
- co2_timeout -> CO2/VOC unavailable;
- th_timeout -> temperature/humidity unavailable;
- individual valid bits and existing documented domain policy.

Use it consistently for hero verdict, live metric labels/selectors, current stats,
Auto editor Current and Auto input handling. Do not mutate received packets.
Test each fault independently and combined, unrelated-sensor preservation,
valid/invalid/recovery and offline while editor open. Preserve existing recovery
hold, sensor-specific Auto suspension and stored valid history.
VOC telemetry0 is allowed in LORA_PROTOCOL; limit-editor min1 is not telemetry
min1. Do not add speculative conversions, health claims or new sensor thresholds.

### R6 — no-op Save must preserve external custom names

on_dialog_save_click unconditionally copies the preset default into cand.name.
Set config name Studio Fan, open Settings, save unchanged -> name becomes
Ventilation Fan and callback fires once. The validation candidate duplicates it.
Preserve name when preset unchanged; intentional changed preset keeps existing
default-name semantics. No new on-screen name editor.
Test actual Save callbacks (both subviews), byte-identical unchanged config and
zero callback/Auto-hold reset; test edited thresholds, changed preset and Cancel.
Use the real24byte name field (max23bytes + NUL), not the false31char test claim.

### R7 — eliminate redundant redraw without suppressing model work

Home CO2/VOC fonts are assigned unconditionally; pinned LVGL style setter refreshes
even if font is equal. Auto Current unit text is also set on every fresh frame.
Independent fixed-tick runs:10 identical Home RX frames ->20flushes/108630pixels;
Auto editor ->10flushes/26230pixels. Pure no-RX/no-time-advance idle ->0.

Guard font/style/text/position updates by actual changed values and only measure
text/reflow units when the necessary value/font changes. Audit equivalent visible
refresh paths. Avoid per-tick allocations and per-frame style attach/detach.
Do NOT skip fresh-frame Auto qualification or delay command/link timeouts merely
because displayed values are unchanged.

Replace print-only performance claims with assertions for:
- fixed-tick identical fresh frames on Home/Trends/Devices and all Auto editors:
  zero flush after layout/cache warmup, when no other visible state changes;
- no-RX subsecond idle away from uptime/timers: zero flush;
- one uptime second: header-only dirty region, stable content untouched;
- one reading change: bounded affected regions; no entire-page redraw;
- command, fault, link loss/recovery and user input still update promptly.

Use a controlled fake node/clock; do not call a case idle if it publishes frames.
Report measured flush rectangles/count/pixels and separate render/touch/transfer
timings; printed nominal zero and host FPS are not measurements of MCU speed.

### R8 — uptime header width change invalidates layout correctly

update_uptime_string writes the shared buffer before update_header compares it;
the alias comparison sees no change.99:59:59->100:00:00 leaves separator gap7px
instead of intended14px. Proportional digits also vary width at same string length.
Use explicit dirty/width tracking, preserving normal header style and right edge.
Test99h->100h,999h->1000h, maximum supported uptime, normal digit transitions and
Connected/Disconnected labels; assert no overlap and intended separator spacing.
Do not relayout the whole page unnecessarily every tick.

## RAM reduction — measure before choosing a smaller reservation

1. Obtain actual MCU part, SRAM bank layout, compiler/linker flags, map + ELF/AXF,
   linker script, target lv_conf.h, display-buffer definitions and RTOS stack/heap
   declarations. Clarify what99.9% counts. Search only task-scoped files; if absent,
   continue safe repo work and report exactly the missing target evidence together.
   Never report a verified MCU percentage from the x86 simulator.
2. Produce a non-double-counted RAM ledger: .data, .bss, .noinit/other RAM sections,
   linker-reserved stacks/heaps, LVGL pool, draw/DMA buffers, RTOS/application heap
   and task stacks. A pool's allocations live inside that pool, not on top of it.
   Verify banks/placement and runtime stack high-water, not only aggregate percent.
   Existing tools/mcu_size.ps1 defaults128KiB/1MiB and sums data+bss; pass actual
   capacities and explain uncovered sections/reservations. Do not infer MCU from
   those defaults.
3. Fix R1 before shrink. Remove24 genuinely unused style initializations/exports
   after checking all consumers. Review's host probe recovers1208B heap; all58
   custom style initializations cost2808B total and descriptors928B host BSS.
   These overlap, are not all additive, and are not ARM guarantees.
4. Convert immutable shared style descriptors/properties to LVGL8.4 supported
   constant-style tables placed in Flash. Keep exact property values, selectors,
   inheritance, ordering and state behavior. Do not mutate/reset const styles.
   Reducing per-object local properties through appropriate shared styles is
   allowed only with pixel-equivalent output. Avoid a complex new theme framework.
5. Removing heap allocations alone does NOT shrink the static43008B LVGL arena.
   After actual reductions, find and document a smaller tested reservation with
   margin. Make profile values explicitly overridable for tested builds if useful;
   keep a conservative fallback. Do not lower the existing >4096B free acceptance
   floor to claim success. Largest free block must cover measured largest required
   allocation with margin; account for creation/render transients and fragmentation.
   Pinned lv_mem_realloc does not maintain max_used accounting: do not size solely
   from lv_mem_monitor.max_used or settled screenshots. Use a bounded diagnostic
   build/instrumentation and report limitations; don't patch vendored production
   LVGL or change allocators just to get nicer numbers.
6. Optional additional optimization: default theme/unused LVGL feature trim.
   Switches/chart/dropdown/labels currently inherit default styling. Only disable
   unused widget features or replace default theme dependencies after explicitly
   reproducing all required default/pressed/disabled/checked styles. Pixel diffs
   and pointer regressions must pass. If this needs a redesign, do not take it.
7. Single draw buffer remains the rule.10lines=16000B;8=12800B saves3200B;
   6=9600B saves6400B;5=8000B saves8000B. Offer/test8 first only if needed.
   Keep10line default until target throughput/latency and DMA constraints validate
   a smaller selection. No second/full-screen framebuffer, extra image cache,
   driver invention or premature flush-ready. Smaller chunks can slow transfers.
   Measure total net RAM and display cost, not just arithmetic savings.
8. Assets are already const (host fonts/icons/splash data+BSS0). Verify target
   read-only placement; do not regenerate/remove accepted icons or shrink fonts
   to save what is actually Flash. Gradient cache is already0; don't claim another
   1024B saving. History is512B here; preserve16samples/metric. Remove dead buffers
   or safely reuse page-local storage only after lifetime proof; never alias two
   simultaneously visible static-label buffers.
9. Choose the least invasive combination yielding real measured headroom.
   Report bytes recovered per independent change plus final total. If target map
   is absent, label smaller-profile results HOST-VERIFIED CANDIDATES, not target
   release approval. Don't promise a fixed KiB goal or arbitrarily cut stacks.

## Frozen behavior and constraints

LVGL8.4,800x480 RGB565; current accepted native theme/icons/fonts/layout.
Visible fonts>=18px; preserve touch targets and pressed/disabled feedback.
One partial buffer; no MCU framebuffer, runtime image/font parsing or new caches.
Process-lifetime static captions and safe fixed buffers. No float/trig formatting
in hot paths. All LVGL/model/config calls serialized in UI/application context;
no ISR LVGL. Preserve six globals defined once in ui.c and existing8B/20B packet
layout, polarity, readback switches and ui_is_tx_ready startup/reconnect gating.

Auto unchanged: Fan CO2 ON>=1000/OFF<=800,0..10000,step50,gap>=50;
Purifier VOC ON>=150/OFF<=100,limits1..500,step5,gap>=5;
Humidifier ON<=40/OFF>=50,0..100,step1,opposite gap>=5.
Three fresh qualifying frames after10s hold; no decision offline/invalid/pending/
latched. No forced OFF on loss, no timed automatic return after manual override.
Retry or explicit Manual->Auto re-arms; edits/reconnect/late report don't clear
error latch. Valid imported non-step-multiple thresholds remain accepted.
No-op Save no callback/reset; Save commits once, Cancel/back Settings discards,
back from Auto keeps draft, reset changes only draft thresholds.
No new Flash/CRC persistence, Node automation, radio/Modbus drivers or UI features.

## Final acceptance and single handoff

Before changes capture/hash accepted native baseline for all27 standard states.
Preserve existing34 checks, adding R1–R8 assertions without weakening thresholds
or comparing only caches. Include mixed50cycle stress (warm cycle10->50 drift
<=256B, free>4096B) AND in-place repeated interaction tests above.
Exercise startup/splash, three pages, all presets, dropdown top/bottom, all three
Auto rules, editable limits/invalid drafts/long press, pending/timeout/Retry,
late reply/offline/reconnect, sensor faults/recovery, polarity and custom names.
Record largest free/fragmentation and transient peaks at final profile.
Check allocation failure diagnostics/assertions; report any skipped target checks.

After LAST relevant edit, run:
- python tools/generate_assets.py --check
- tools/build_sim.bat
- tools/run_regression.bat
- tools/export_mcu.bat --verify
- tools/verify_production.bat

Run new tests, final captures, pixel comparison, generation/source identity and
memory/performance measurements in that same delivery. No staged prompt or
owner-mediated per-issue review. Fix regressions introduced by optimizations
before handoff. Identical normal-state RGB565 images are expected; R2/R3/R4/R5/R8
fault/boundary corrections are explicit allowed differences, documented by state.
No promise of zero bugs or success for unrun hardware tests.

One dated docs/reviews/<delivery>/ report: changed files, R1–R8 reproductions and
passing assertions, actual command logs, code/executable/config hashes, native
captures, per-change + total RAM ledger, draw/timing measurements, tested profile
and target limitations. Correct stale integration guidance (gradient1024, old
font/icon counts, historical zero-flush figures, stale visual rejection) without
rewriting historical evidence as if new tests existed.
Update PLAN, DEV_LOG, UI_DESIGN_BRIEF, MCU_BASELINE, README/indexes and test guide.
CURRENT stays the only active handoff; mark IMPLEMENTED — awaiting review after
completion, with target sign-off pending stated explicitly if still missing.
