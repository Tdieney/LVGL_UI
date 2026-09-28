# Accepted UI — runtime, performance and RAM review

2026-09-21. **CHANGES REQUIRED for runtime defects, not a visual redesign.**
The owner accepts the current native appearance after working with the developer.
Older requests to restore the original concept's geometry/colors are superseded.
One corrective/optimization handoff: [CURRENT](../../prompts/CURRENT.md).

## Evidence and coverage

- Reviewed the current production UI, Auto/wire/config/history paths, theme and
  generated assets, simulator/test changes, asset and export/verification tools.
  The worktree includes several previous deliveries; do not assume every dirty
  line is from the latest alignment change. Historical prototype assets are not
  the active runtime or acceptance reference.
- Fresh simulator configure/build: PASS (no compilation needed; one upstream
  CMake CMP0177 development warning). [Source/executable hashes](SOURCE_IDENTITY.json).
- Fresh regression: **34/34 PASS**, smoke PASS and27 standard captures emitted.
  Full output: [REGRESSION.txt](REGRESSION.txt).
- `python tools/generate_assets.py --check`: PASS,25 masters/68 runtime icons.
- `tools/export_mcu.bat --verify`: PASS.
- `tools/verify_production.bat`: all four phases PASS, including isolated host
  build, production smoke and deliberate negative control. Initial sandbox run
  was blocked by taskkill access denial; permitted outside-sandbox retry passed.
- Additional independent [probe.c](probe.c) compiles the actual ui.c without
  production edits; [PROBE.txt](PROBE.txt) records reproducible failures missed by
  the suite. It uses direct callbacks/API calls and actual LVGL rendering, not
  pointer-input emulation. Its framebuffer exists only in this host diagnostic.
- Fresh standard RGB565 captures are under captures/. Visually inspected native
  Home/Settings and independent defect captures; no demand to change accepted art.
- No target firmware .map/.elf/.axf/linker script or target lv_conf.h was found
  in the searched workspace outside cached dependencies/review fixtures. Asked
  owner for MCU/model/config/map and meaning of99.9%; none available at delivery.
  No target deployment, FPS/latency or whole-MCU RAM claim is certified here.

## R1 — P1: repeated interactions accumulate duplicate style references

Two reachable paths keep appending the same style to an existing object:

1. ui.c:1306 update_trends_tabs removes the opposite style but always adds the
   desired one. ui_select_trend_metric:3489 also calls it for the already selected
   metric; on a real metric change update_trends_page_widgets calls it again.
   Twenty repeated selections of CO2: free10088→8792B (-1296B), selected card
   style_cnt4→24 in the final independent probe. Earlier isolated ordering gave
   -1312B; allocator placement explains the small difference.
2. ui.c:2314 on_dialog_preset_dropdown_event adds five list styles and one label
   style on EVERY LV_EVENT_READY. LVGL8.4 lv_dropdown_close only hides the list;
   reopening reuses it. Eight additional open/close cycles in ONE editor:
   free14328→13552B (-776B); list style_cnt16→56.

Pinned LVGL lv_obj_add_style unconditionally increments style_cnt and reallocates
the style array; it does not deduplicate. lv_obj_t.style_cnt is only6bits.
Continued growth risks count overflow/memory corruption, not just wasted RAM.
No deliberate overflow/crash was run. Existing navigation stress destroys objects
between cycles and misses growth inside a persistent page/editor.

Required: attach once per object/selector or update only on state transitions;
repeated selected taps must be a visual/heap no-op. Regression must repeat within
the same live object tree, not recreate it to hide the growth.

## R2 — P2: Trends text caches have two writers for one label

ui.c:1665–1671 and1747–1751 set the SAME value label first from legacy formatted
number+unit buffers, then numeric-only buffers. set_buffer_and_label_if_changed
compares the buffer, not whether the label still points at that buffer.

Reproduce CO2=100 then select RH=100 with histories also100: Current,Min,Max
become `100%` while a separate `%` label remains. The unchanged numeric buffer
causes the second writer to skip. [Native proof](captures/probe_duplicate_units.png).
The next changed numeric reading fixes Current but not necessarily Min/Max.
Required: exactly one authoritative text buffer per actual label; legacy formatted
test/API buffers may be updated without assigning them to numeric UI labels.

## R3 — P2: Trends range cache survives destruction of the chart

ui.c:1432 resets metric/history revisions but not s_last_trends_range_min/max.
ui.c:1851 skips setting the new chart range if cached numbers match. New charts
start at0..100. Independent RH history100: first chart96..104; leave and return:
actual chart0..100 but labels/cache still96..104. A plot can misrepresent readings.
Required: initialize range on every chart instance or invalidate instance-local
range cache; assert actual chart range and pixels after a same-history rebuild.

## R4 — P2: isolated valid points disappear unless last/inspected

ui.c:1364 on_chart_draw_part_begin makes every non-highlighted point transparent,
then explicitly draws isolated points with that SAME transparent descriptor.
History [400,invalid,450,invalid,500] contains three isolated valid readings;
9x9 probes around their LVGL point coordinates find forest pixels0,0,29.
[Native proof](captures/probe_isolated_points.png) shows only the final dot.
Required: explicit visible marker for EVERY isolated valid point (gaps retained),
while retaining the accepted last/inspected emphasis and normal line appearance.

## R5 — P2: display validity contradicts the Auto engine's fault handling

Home (ui.c:1095), Trends (1624) and Auto live reading (2475) check validity bits
but ignore relevant rs485/co2_timeout/th_timeout flags; ui_auto_eval correctly
masks those faults. With valid bits1 plus rs485_fault1 the probe shows air `Good`,
CO2 `420`, VOC `80` and editor Current `420`, despite an unusable sensor bus.
Required: a consistent read-only availability predicate, relevant fault masking
and documented range policy; do not clear wire bits or change packet semantics.
Unrelated TH faults must not hide otherwise usable CO2/VOC. Wire VOC0 remains
allowed per LORA_PROTOCOL; do not confuse ON/OFF limit minimum1 with telemetry.
Preserve stored history; unavailable live data must not erase past valid samples.

## R6 — P2: no-op Settings Save overwrites custom device names

ui.c:2773 always replaces cand.name with the preset default. Setting a supported
external config name `Studio Fan`, opening Settings and saving unchanged produces
`Ventilation Fan` plus one config-changed callback. This violates no-op Save and
destroys an existing supported config field. ui.c:2461 duplicates the assumption.
Required: preserve current name when preset is unchanged; retain the existing
default-name behavior for an intentional different preset. No new name editor.

## R7 — P2: redundant rendering on identical received frames

ui.c:1152/1174 reapply Home CO2/VOC font unconditionally. Pinned LVGL
lv_obj_set_local_style_prop refreshes style even if value is equal. ui.c:2504/2514
also reapplies the Auto live unit text. Independent test at a fixed tick, stable
link, no command/animation changes:

| Case | Flush calls | Pixels |
|---|---:|---:|
|10 identical Home received frames|20|108630|
|10 ticks with NO received frame or time advance|0|0|
|10 identical CO2 Auto editor received frames|10|26230|

Main regression separately prints106500pixels for10 identical Home frames and
still passes: its performance block is observational, not asserting zero.
Its “idle” scenarios also need isolation from fake-node updates/timer boundaries;
do not describe their nonzero result as proof that pure idle always redraws.
Required: guard font/text/style changes, keep Auto evaluation on every fresh frame,
and assert no pixel traffic when nothing visible changes. Do not throttle Auto or
drop input reports merely to improve a graphics benchmark.

## R8 — P2, long-session boundary: uptime widening does not realign header

ui.c:625 updates the shared buffer before calling update_header when text length
changes. update_header:736 compares the already updated label/buffer and decides
up_changed=false; initialized/stable-link header never relayouts. Probe gap to
separator changes26px at99:59:59 to7px at100:00:00, rather than preserving the
specified14px gap. Proportional digits also vary width without a length change.
Required: explicit width/layout dirty signal or measured width change, not buffer
alias comparison. Retain existing header positions/typography in normal states.

## RAM findings and safe optimization order

**99.9% remains an owner report, not a diagnosed whole-MCU measurement.**
A linker can count a whole reserved heap/stack as occupied while that heap still
has runtime space. Conversely free LVGL heap says nothing about another task's
stack or an overflowing different SRAM bank. Need both map and runtime watermarks.

| Allocation/evidence | Current observation | Meaning |
|---|---:|---|
|LVGL pool, ui_mcu_profile.h|43008B|Static reservation when LV_MEM_CUSTOM=0|
|One800x10 RGB565 draw buffer|16000B|Declared in sim/port; actual MCU allocation must be checked|
|Pool + draw baseline|59008B|Excludes UI globals, firmware, stacks, other heaps/DMA|
|CHECK8 smallest free|4216B; largest2416B|Only120B above existing4096B free threshold|
|CHECK31 steady50cycle smallest free|4760B; largest minimum3280B|Not proof against R1 repeated in-place interactions|
|58 custom style descriptors|928B host BSS|Not target sizeof; includes24 unused styles|
|All custom theme initialization|2808B host heap cost|Separate --theme-only process:40408→37600B free|
|24 initialized but unused styles|1208B host heap recovered|Diagnostic lv_style_reset only; overlaps the2808B total|
|Fonts/icons/splash host objects|100336/39424/48992B text;0data,0BSS|Already const; mostly ROM candidates, not194KiB of RAM saving|

Prioritize:

1. Fix R1 growth before decreasing any budget. Remove unused custom styles; move
   immutable remaining style property tables to LVGL8.4 constant styles in ROM.
   Audit actual style references/inheritance/state combinations; don't remove
   styles merely because they resemble old colors. Remaining theme heap portion
   is1600B host by subtraction, not an additional guaranteed target saving.
2. Only after reducing peak allocation, reduce the reserved LV_MEM_SIZE with a
   measured safety margin. Freeing1208B INSIDE a fixed43008B pool does not itself
   reduce that pool's linker allocation. Don't double-count internal allocations
   as separate RAM on top of the pool.
3. Consider eliminating unused default-theme widget styles/config features or
   explicitly owning all needed widget styles. Default-theme initialization costs
   RAM; however current switches/chart/dropdown/labels inherit it. Blindly disabling
   LV_USE_THEME_DEFAULT changes accepted appearance/interaction. Only take this
   optimization if all state screenshots and pointer tests stay equivalent.
4. Smaller single partial buffers are a measurable fallback:800x8 saves3200B;
  800x6 saves6400B;800x5 saves8000B versus10lines. These are exact buffer arithmetic,
   NOT measured FPS or total net savings. Smaller chunks can increase transfer/setup
   overhead. Keep10line default until target profiling/acceptance; no double/full
   framebuffer. Check DMA alignment, source lifetime and flush-ready timing.
5. Remove truly dead globals and share scratch buffers only when label lifetime
   makes it safe. Page-local pointer/buffer reuse is lower priority than styles.
   History is only4*16*sizeof(sample)=512B here; don't cut history or text/icons
   for negligible RAM savings and a worse product. Min font18px remains fixed.

Gradient cache is ALREADY disabled in ui_theme_init and sim_pc/lv_conf.h. Old docs
claiming a1024B active gradient cache/Segoe/46icons/zero identical-frame redraw
are stale. Do not claim saving it again or reintroduce gradients.

The host ui.c object is576B data+4096B BSS (test hooks enabled); ui_theme928B BSS.
These are indicators, not ARM linker figures. PE/COFF nm inferred symbol sizes
are unsuitable for per-symbol RAM accounting here; use the actual target map.
Pinned lv_mem_realloc does not update cur_used/max_used, so lv_mem_monitor.max_used
alone is not a reliable total allocation high-water for a style-realloc-heavy UI.
Measure free/largest during allocation-sensitive paths and use bounded diagnostic
allocation instrumentation where needed, without patching vendored production LVGL.

## Performance / target validation

Keep callbacks short, UI/model publication in one serialized context, bounded
dirty regions and tick scheduling. Instrument draw CPU time, handler latency,
flush count/pixels/bytes, transfer duration and input-to-visible latency on target.
Do not equate host rendering time or forced lv_refr_now calls with MCU FPS.
No preset global “60fps” timer, bigger image cache or dual buffer as a blanket fix.
Relevant primary references: [LVGL8.4 drawing/buffers](https://lvgl.io/docs/open/8.4/overview/drawing.html)
and [LVGL8.4 styles](https://lvgl.io/docs/open/8.4/overview/style.html); actual
version-pinned code under sim_pc/build/_deps/lvgl-src governs the findings above.

ui-ux-pro-max was used for review discipline/interaction consistency. Its focused
performance search returned mostly web guidance; no verified LVGL-specific skill
match was found, so CSS transform recommendations were not imported. RAM/rendering
conclusions come from compiled probes and pinned LVGL, not generic design advice.

## Reproduce the independent probe

From repo root, with the existing simulator LVGL library built:

```powershell
& C:/Toolchains/w64devkit/bin/gcc.exe -std=c11 -O2 -Wall -Wextra -Werror -DLV_CONF_INCLUDE_SIMPLE -Isim_pc -Isim_pc/build/_deps/lvgl-src -I. docs/reviews/2026-09-21-runtime-ram-review/probe.c ui_auto.c ui_theme.c ui_icons.c ui_fonts.c ui_splash_logo.c sim_pc/build/lib/liblvgl.a -o docs/reviews/2026-09-21-runtime-ram-review/probe.exe
& docs/reviews/2026-09-21-runtime-ram-review/probe.exe
& docs/reviews/2026-09-21-runtime-ram-review/probe.exe --theme-only
```

Probe logs observations and exits0; it is NOT a passing acceptance test. It also
resets unused styles at the end of its own process solely to measure reclaim.
Developer must turn defect reproductions into failing-then-passing assertions.
No production source/asset was changed, no commit/push. Physical MCU fit and
smoothness remain unverified until target evidence exists.
