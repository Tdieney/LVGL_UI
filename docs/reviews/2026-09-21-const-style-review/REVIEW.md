# Independent const-style / runtime review — 2026-09-21

Disposition: **CHANGES REQUIRED**. One [CURRENT](../../prompts/CURRENT.md).
Review only: production sources/defaults unchanged; new diagnostic files and
documentation only. No commit/push. Author submission:
[delivery report](../2026-09-21-runtime-followup-delivery/REVIEW.md).

## What passes and stays accepted

- Fresh simulator build; **34/34** normal regression, asset integrity
  (25 masters/68 runtime icons), export manifest and all four isolated production
  verification phases PASS. Production verifier used approved escalation for
  its owned-process cleanup, as needed in the previous round.
- The old uptime widening reproduction is fixed: reconnect111:11:11 gap59px,
  next111:20:00 gap37px; clock position stays stable at equal string length.
- Dashboard Temp/RH has dash with no Offline/No data or unit; both device pages
  remove Unknown, keep a single dash and retain Retry/pending semantics.
  Trends messages remain. [Offline](captures/home_offline.png),
  [error+Retry](captures/devices_error.png).
- Prior Trends numeric ownership/range/isolated-marker repairs remain:
  Current/Min/Max100 with separate%, chart96..104 after rebuild, isolated marker
  forest pixels13/13/29. Bus/individual timeout/domain matrix remains coherent.
- Identical Home/all three valid Auto editors:0flush/0pixels after warmup.
  Custom-name no-op Save still callback0. 200 same-selected tabs: no heap drift.
- Theme-only initialization now costs0 LVGL heap bytes; diagnostic Home boot
  free8464B vs previous6896B. This is a real internal-headroom improvement.
- Native RGB565 pixel comparison with the prior reviewer captures: normal page
  content below y56 is identical (Home good/moderate/poor, Trends all four
  metrics/empty, Devices, Settings, Auto variants, dropdown open). Content
  differences occur in the explicitly approved unavailable/error copy areas;
  header changed, including the regression below. Splash identical.
- ui-ux-pro-max applied for readability/state/interaction review only, no redesign.

## G1 — P2: Connected/Disconnected uses stale text width and clips screen edge

Location ui.c:update_header, about lines815..865, especially851.
The previous lv_obj_update_layout call was removed while link text still changes
immediately before lv_obj_get_width. Width is stale until the layout pass.

Fresh native reproduction:
1. Connected Home, invalidate link timestamp, ui_tick and render.
2. Actual Disconnected coordinates **x678..800**, width123. Screen ends799;
   intended rightmost text pixel is777 (22px padding).
3. Reconnect then render: Connected sits **x655..754**, leaving45px padding.
   Header positions were computed with the previous label's width in each case.

[Offline capture](captures/home_offline.png) visibly clips the last character.
This is a new header regression, not the old uptime defect reopened.
Update/measure the actual new link text before computing the right-aligned group,
or use a correct reserved link slot. Preserve stable uptime reservation, minimum
14px separator clearance and no whole-page redraw. Add actual rendered link
bounds tests across loss/recovery and initial offline startup, not just uptime gap.

## G2 — P2: hand-written constant style initializers break alternate LVGL configs

Locations ui_theme.c:5..17 and every style definition; ui_theme.h:62 onwards.
The project exports to firmware-controlled lv_conf.h, not only simulator config.

1. With LV_USE_ASSERT_STYLE=1, all hand-written descriptors omit sentinel.
   [style_config_probe.c](style_config_probe.c) compiled for that config prints:
   actual00000000 vs expectedaabbccdd and exits1.
   Pinned lv_style_get_prop calls LV_ASSERT_STYLE; enabling this supported debug
   check will reject these descriptors. The normal simulator has it disabled.
   Use pinned LV_STYLE_CONST_INIT, which initializes sentinel conditionally.
2. UI_HEX hardcodes an unswapped full RGB565 word. With LV_COLOR_16_SWAP=1,
   canvas style isef7c while lv_color_hex(0xEEECE7) is7cef. Probe exits1.
   This mixes incompatible representations between shared styles and the
   existing runtime palette. Use pinned LV_COLOR_MAKE-compatible constant
   initialization (or equivalent correct configured representation), not a
   hardcoded native-endian word. Keep RGB565; no new color-depth scope.

These are independently checked descriptor/color-data failures; the tiny probes
do not run an alternate-config full LVGL UI. Their library is linked for unused
symbols, not used for mixed-ABI style calls. Full consistent-config integration
builds remain an acceptance requirement. No claim that the current MCU has
either option enabled.

Also the 34 style DESCRIPTORS are not C const: nm reports D for all and host
ui_theme object is text2144/data544/bss0. Property arrays are read-only, but
descriptors still consume writable memory. Native LV_STYLE_CONST_INIT declares
const lv_style_t; match extern declarations and helper parameter qualifiers.
Do not cast away const or disable assertions to get a passing build.

## G3 — incomplete/misleading RAM closure, with a useful measured candidate

The author's ledger labels59008B as total static UI BSS, omitting active style
descriptors and other UI/app state. Correct limited-component accounting:

| Host component | Before this delivery | Submitted |
|---|---:|---:|
| LVGL fixed pool |43008|43008|
| One10-line draw buffer |16000|16000|
| Theme descriptor data+BSS |928|544|
| Subtotal of ONLY those three components |59936|59552|

Net384B writable descriptor reduction is real; don't count old BSS disappearance
again as a544B saving when it moved to .data. Theme heap headroom is INSIDE the
43008B arena. Actual target percentage/sections/stack placement still unknown.

Fresh default42KiB suite: CHECK8 min free7144/largest5344;
CHECK31 min settled free7592/largest6104, drift0.
The author reported7608 in its own run; scenario/allocator order can differ.

No smaller-profile build/log was supplied. Arithmetic about38KiB in MCU_BASELINE
does not substitute for a candidate trial, nor justify declaring8-line transfers
safe without target timing. Reviewer independently built **40KiB** with the same
pinned LVGL in build-40k, no production edits/default change:
- current34/34 regression PASS;
- CHECK8 Home free5080/largest3280;
- CHECK31 min settled free5480/largest4000, drift+72B (within existing limit);
- pool40960 + draw16000 =56960B, **2048B less reservation** than default.
[Full candidate regression](REGRESSION_40K.txt).

This is a promising HOST CANDIDATE, not approval of MCU RAM99.9% or allocation
transients. Extend final tests/instrumentation after actual fixes, measure peak
allocation/fragmentation and largest required block, then document a tested
selection with conservative42KiB fallback. Do not lower>4096B floor. Target
map/config/stacks/DMA/timing remain external limits; no need to redesign UI.

## G4 — remaining requested proof is still not in durable tests

Do not treat an observational probe's exit0 as a pass/fail gate.
Examples in current main.c:
-34h (5709..5719) checks y_points values, NOT isolated marker pixels/no bridging.
  The original transparent-marker bug would pass that check.
-34i (5722..5749) checks bus fault on Home and ONE Auto editor, not the
  advertised full sensor/Trends/editor fault matrix.
-34j (5755..5767) checks name only; despite report claiming callback assertion,
  this test never measures no-op callback count or preserved config/Auto hold.
-Zero-redraw assertion is Home only, not all three Auto editors.
-34k checks uptime gap but not current link text screen containment, missingG1.

Actual visible Trends RH assertions and unavailable-state tests are improved.
Keep them and all existing behavior tests. Complete the focused missing checks
from the prior CURRENT in the SAME final delivery, using real rendered labels,
pixels, callbacks and controlled clocks. Include pointer stress after warmup.
Do not reimplement production repairs already independently working.

## Evidence and limits

[REGRESSION.txt](REGRESSION.txt), [PROBE.txt](PROBE.txt),
[CONFIG_PROBES.txt](CONFIG_PROBES.txt), [VERIFICATION.md](VERIFICATION.md),
[SOURCE_IDENTITY.json](SOURCE_IDENTITY.json). Probe logs observations, not assertions.
Current source comparison/evidence is scoped to this delivery; dirty HEAD
includes older work. No physical LCD, target memory map, stack watermarks or
hardware throughput measurements. No new styles/icons/product features requested.
