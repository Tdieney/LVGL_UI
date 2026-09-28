> ARCHIVED — historical context only. Active handoff: [CURRENT](../CURRENT.md).
> Superseded after independent reconstruction review on 2026-09-18; implementation claims are historical, not acceptance.

# CURRENT — reproduce the approved design, redraw its icons

Date: 2026-09-18. Status: **IMPLEMENTED — awaiting review**.
Consolidated delivery implemented in full (R1–R7 + V1). See [Delivery Report](../../reviews/2026-09-18-quiet-reconstruction/REPORT.md)
and [Interactive Comparison](../../reviews/2026-09-18-quiet-reconstruction/COMPARE.html).
ONE delivery for R1–R7. Owner rejected the second implementation and explicitly
requests resemblance to the original approved images, including regenerated icons.
This is reconstruction, not permission to invent another layout/design system.

## Authority, scope and reading

Read AGENTS.md, docs/prompts/README.md, docs/README.md, docs/HARDWARE_OVERVIEW.md,
docs/MCU_BASELINE.md, docs/UI_DESIGN_BRIEF.md, docs/user/SMART_HUB_UI_GUIDE.md,
docs/user/LORA_GUIDE.md and LORA_PROTOCOL.md.
Read [R2 review](../../reviews/2026-09-18-fidelity-r2-review/REVIEW.md) and
[visual comparison](../../reviews/2026-09-18-fidelity-r2-review/COMPARE.html).

Actually VIEW all seven files in docs/drafts/2026-09-17-pro-max/:
dashboard-quiet-architecture.png; trends-quiet-architecture.png;
devices-quiet-architecture.png; settings-quiet-architecture.png;
auto-co2-quiet-architecture.png; auto-humidity-quiet-architecture.png;
auto-voc-invalid-quiet-architecture.png.

These images and latest owner requests govern visuals, NOT the submitted native
renders/MATCH claims. Do not use dashboard-concept.png or the Study12 browser UI.
Use ui-ux-pro-max if available, but do not generate a new design direction.
The18px floor overrides tiny concept text, not the entire approved composition.

Inspect the full working diff. Preserve dirty owner work; snapshot/hash starting
sources. No commit/push/branch/reset/clean/stash, packet changes, drivers, Flash/
CRC, Node automation or new product features. Implement real native controls,
not screenshot backgrounds plus hitboxes. All fixes, self-review and final
regression belong in this one delivery; no owner-mediated phases.

## R2 — restore and lock composition

The table is a technical-lead800x480 translation target, not a claim the raster
was exact CAD. Small4–8px fit adjustments are allowed with an explicit reason.
Do NOT collapse header/tab rows or change major column proportions again.

| Region | Native target |
|---|---|
| Header | x24..776,y0..56; prominent Studio/home left, uptime/status right |
| Tabs | separate y56..100 row; Overview/Trends/Devices aligned left |
| Main content | x24,y112,w752,h344; bottom24 |
| Overview | left416 +gap16 +right320; NOT equal368px columns |
| Hero / lower tiles | hero416x216; gap16; two200x112 environmental tiles |
| Devices |2x2 cards368x164, gaps16 |
| Trends | approximately184 rail +16 gap +552 chart, NOT240/496 |
| Settings | context title below header; form x24,y112,w752,h280; actions y408,h48 |
| Auto | title below header; strip x24,y108,w752,h80; two368x148 cards y200; helper y356,h52; actions y416,h48 |

- Tabs are NOT inside the Studio row and NOT in a gray segmented tray.
  Only the selected label has a celadon capsule.
- Studio approximately34px, tabs24px, strong headings/readings with subordinate
  labels/units. Use a coherent small font set; >=18px ALL visible text/glyph
  fallbacks, including axes, errors, disabled values and dropdowns.
- Respect actual font metrics: font34 line height46, font48 line height65.
  Do not size containers using nominal point size alone.
- Hit regions >=44px; steppers/actions >=48px; independent actions separated
  by8px. Current90x30 tabs and36x36 shortcut are not acceptable.
- Header must fit Connected/Disconnected and100/1000-hour uptime without
  navigation occluding timer/status. Never solve by reducing below18px.
- Restore warm neutral canvas/porcelain, near-black/forest ink, muted celadon,
  restrained fine outlines and subtle tonal depth. Match original forest
  variation with bounded rendering where feasible; no glow/heavy borders.
  Do not treat flat recoloring as a substitute for composition and assets.

## R3 — redraw/regenerate the icon family (REQUIRED)

The current44px variants reuse old paths. Merely resizing those is not a fix.
Keep editable SVG sources or organized vector paths in the generator:

| Icon | Required silhouette from reference |
|---|---|
| Purifier | Cylinder, elliptical top rim, curved base, sparse perforations; not calculator rectangle |
| Fan | Four balanced rounded blades around small center; not three-blade pinwheel |
| Humidity | Clean symmetrical teardrop, no inner swirl |
| Desk light | Connected upright stem/base and angled head; not broken diagonal stand/rays |
| Sliders | Three tracks with circular knobs; not two tiny tracks |
| Home/thermometer/wind/leaf/gear/warning/check | Coherent optical weight and rounded stroke family matching concepts |
| Navigation/steppers | Proper chevrons and large centered plus/minus, not tiny text '<'/'>' |

Cover ALL existing presets coherently, including generic/socket/heater/
dehumidifier. Update icons on every page when preset changes.
Native roles: roughly40–48px appliances in Devices/editors,32–36px in compact
Overview rows,24–32px controls,32–40px sensors. Match actual ink bounds and
optical mass, not only canvas dimensions. Near-black/forest; no colored plates.

Generate antialiased recolorable alpha masks at real output sizes from vectors.
Do not upscale old raster, hand-edit generated C only, regenerate splash
unnecessarily or add a full-screen bitmap. Synchronize source/generator,
declarations/build/export. Deliver a native-size contact sheet plus enlarged
silhouette comparison; inspect it and page renders yourself in this delivery.

## R1/R4/R5 — exact page corrections

### Overview

- Restore secondary Air quality, dominant verdict, check/status badge, wind
  symbol, two metric columns with VERTICAL separator. Large values/smaller ppm.
- VOC has NO index/ppm/ppb suffix. Remove “within healthy range”; retain neutral
  category semantics, no health assurance or new calibration/rules.
- Position units by measured digit width/baseline, never fixed x92/x65.
  Test420/850/1420/10000/full valid wire CO2 range;85/160/340/500 VOC; unavailable.
  No numeric clipping, overlap, ellipsis or misleading truncation.
- Shallow environmental tiles: icon left, title/value in adjacent aligned
  column, units smaller. No current three-tier vertical icon/title/value stack.
  Preserve signed-temperature extremes and offline/invalid labels >=18px.
- Right panel: reference heading scale, centered rows/switches, name/mode text
  column, clear separators/shortcut. Concise display name Ventilation allowed;
  do not change stored identifiers or appliance semantics.
- Auto paused MUST be completely visible, not clipped/wrapped in98px lane.
  Also fit Auto+sensor/Sending/Unknown/Retry. Allocate distinct lanes and
  preserve separate switch/Retry targets and readback-driven state.
- No notification panel. Short existing feedback may use unused nav-row space
  or inline placement, NEVER obscure the Devices shortcut/header/controls.

### Trends

- Narrow selectors with sensor ICON, label and prominent value/subordinate
  unit. Icons are still missing in the current submission. Keep text >=18px;
  calculate card baselines using actual font metrics.
- Short chart headings CO2/VOC/Temperature/Humidity, no VOC index or unnecessary
  concentration wording. Current/Min/Max distinct columns, separated units,
  enough width for max/negative/no-data values. Check all four metrics.
- Subtle forest line/area, emphasized endpoint/inspected sample, not equal6px
  dots everywhere. Preserve real16-sample data/gaps/point inspection.
- REMOVE literal Last15minutes; derive coverage when shown, omit when empty.
  Truthful existing timestamps allowed; relative labels must use real timing
  and day rollover. Never synthesize samples/window just to mimic an image.
- Empty/partial/invalid history and selector/Current validity must agree.

### Devices

- Restore compact reference card proportions; remove oversized empty middle.
  Large coherent icon, title/mode group, approximately64x36 painted switch
  with safe hit area. Auto leaf +sensor association, Manual without ON/OFF.
- Proper three-track Settings icon, prominent readable caption, real chevron
  and shared gutters/separator. No colored plate/inset filled action.
- Preserve all presets and pending/unknown/paused/Retry states. Manual remains
  production default; use explicit fixtures for Auto comparison images.

### Settings

- Keep improved contextual title and joined mode/polarity choices.
  Restore coherent row icons,24px control values where fitting, outlined
  dropdown and visible chevron (current style is still border0).
- Explicit ON/OFF association in limits summary; use compact two-line layout
  if needed within row, especially for inverse RH. No unit truncation.
- Unsupported preset: short “Manual only”; disabled Auto/threshold entry.
  Parent invalid-draft hint must remain visible.
- Match reference Cancel/Save hierarchy; do not force unrelated two-action
  footer to share the Auto three-column layout.

### Auto

- 80px sensor strip: Current label and fully visible live number/subordinate
  unit. Remove duplicated Now. Test valid, unavailable and largest readings.
  Account for padding, ascent/descent and ALL clipping ancestors.
- Symmetric cards, prominent Turn ON/OFF, inclusive comparator, outlined
  numbers with smaller units; large optically centered +/- in >=48px targets.
- Preserve stable field-only red validation; VOC150/150 has warning, cause,
  repair instruction and disabled Save. RH repair directions reversed.
- Two18px helper/error lines fit reserved slot without moving cards/footer.
  Three equal48px actions and consistent gaps. Check0/100RH,1/500VOC,10000CO2,
  imported non-step multiples, disabled range ends and real pointer edits.

## R6/R7 — proof and reporting corrections, in the SAME delivery

Preserve real improvements: streamed ZIP hashes, same-length mismatch negative
control, ordinary-time boundary test, larger font assets, segmented controls,
stable error outlines. Close only remaining obligations:
- Exact ZIP membership/duplicates, every required file/doc hash in both default
  archives and explicit winner; reject unexpected/duplicate entries. Preserve
  atomic non-overwrite and sentinel safety.
- Both timeout helpers: record task-owned worker/child PIDs, assert bounded
  termination, fail on cleanup/taskkill/wait failures, retain stdout/stderr/
  exit diagnostics and sentinel hashes. Exception strings alone prove nothing
  about remaining processes. Disposable canonical-path-checked fixtures only.
- CHECK1: real LVGL clock crosses UINT32_MAX through ui_tick/integration TX at
 4999/5000ms, masks stale display, refuses user/Auto, records zero stale TX,
  then recovers. Ordinary-time path plus separate predicates is not equivalent.
- CHECK28: assert actual PENDING then timeout/latch BEFORE second retry recovery;
  keep actions/pages/polarities/recovery/edge-center exactly-once matrix.
- CHECK31: actual sequence/TX/readback in both directions; assert ALL five
  scheduled lifecycles. IDLE/nonzero counters insufficient. Keep50 cycles,
  >4096 free, abs(drift)<=256; fragmentation/largest-block diagnostic only.
- Presentation tests must check visible glyph containment in clipping ancestors
  and sibling lanes, not merely locate label objects. Add tests for R1 cases,
  hit targets, long chart values and stable validation. Keep18px audit/negative
  control. Remove hidden labels retained solely for presentation assertions.


## Frozen functional and MCU contract

- LVGL8.4,800x480 RGB565,42KiB heap, ONE800x10 partial draw buffer. No MCU
  framebuffer, heavy compositing or per-tick allocation/float/trig.
- Static lifetime captions with lv_label_set_text_static; fixed dynamic buffers;
  bounded change-guarded redraw, LVGL only in application context.
- Keep page widgets released while focused editors are open; model/history/
  command tracking and Auto survive page rebuilds. Preserve pointer safety.
- Restore restrained visual depth/gradient only with a bounded implementation
  compatible with current allocation/cache constraints. Do not blindly enable
  gradient caching or allocate a large intermediate image. Measure transient
  and settled memory; retain >=18px and component hierarchy if effects need a
  documented low-cost approximation. Texture/photorealism is not required.
- ui.c defines the six globals exactly once: lora_hub_cmd, lora_node_status,
  lora_last_rx_tick_ms, lora_rx_revision, lora_last_snr, lora_last_rssi.
  Headers extern; firmware/simulator do not redefine. Plain #include "lvgl.h".
- Hub owns semantics/polarity/Auto; Node raw GPIO. Existing8-byte command and
  20-byte status, ui_is_tx_ready gating and direct wire globals unchanged.
- Uptime (not wall clock), Connected/Disconnected, signal bars, splash, English,
  °C and session history remain. No Details tab, notification panel, invented
  AQI/ppb-to-index algorithm or new health guarantee. Short operational feedback
  may remain if it does not obscure navigation/content or replace inline faults.
- Relay switches reflect Node readback, not optimistic intended values.

| Auto rule | ON / OFF defaults | Bounds | Step | Inclusive gap |
|---|---|---|---|---|
| Fan / CO2 ppm | >=1000 / <=800 | 0..10000 | 50 | ON>=OFF+50 |
| Purifier / unitless VOC | >=150 / <=100 | 1..500 | 5 | ON>=OFF+5 |
| Humidifier / %RH | <=40 / >=50 | 0..100 | 1 | OFF>=ON+5 |

Imported valid integers need not be step multiples. Preset change loads defaults;
unsupported types Manual-only. Missing/invalid/legacy config defaults Manual;
valid explicit Auto opts in. No new CRC/persistence guarantee.
Three eligible fresh frames plus10s hold after enable, committed limit change,
confirmed transition, boot/reconnect and sensor recovery. No Auto decision
offline/invalid/pending/latched. Manual override has no timed Auto return.
Retry or deliberate Manual->Auto re-arms; ordinary edits/reconnect/late ACK do
not. No forced-OFF on link loss. No-op Save emits no callback/reset.
Back from Auto preserves shared draft; Back from Settings/Cancel discards;
Save commits once. Reset changes draft thresholds only.

## One final acceptance and delivery

1. Self-review the full final diff; finish all in-scope regressions yourself.
   No separate phases/prompts or owner-mediated issue-group reviews.
2. Capture native800x480 versions of all seven concepts with matched TEST
   fixtures: Auto modes on Overview/Devices/Settings, CO2live420, RH48,
   VOC150/150 invalid. Do not change production defaults for this.
   Also capture all four Trends metrics and the fault/boundary matrix above.
3. Provide same-size reference/native comparison and optional HTML opacity
   overlay. Document target/actual anchors and justified differences. Do not
   rely on whole-image similarity scores for readability/safety or label every
   row MATCH.18px adaptation/texture are exceptions, not whole-page redesign.
4. Inspect actual renders and icon contact sheet. No clipped glyphs/hidden
   states/overlapped units. Visual acceptance belongs to owner/reviewer.
5. After last relevant edit run cached:
   tools/build_sim.bat
   tools/run_regression.bat
   tools/export_mcu.bat --verify
   tools/verify_production.bat
   Keep C99 production include/link without UI_TEST_HOOKS/include-simple macro;
   strict negative control must launch, exit1 and emit expected stderr.
6. Report immediate/settled heap (including new effects/dropdown/error states),
   drift, no-change redraw/dirty regions. Measure compiled font/icon sections
   for Flash cost, not C source text size. Icons are alpha masks. Manifest-only
   check does not hash ZIPs. No actual-target acceptance inferred from PC tests.
7. Save one report, command logs, all screenshots and contact sheet/comparison
   under docs/reviews/<date>-quiet-reconstruction/. Preserve previous evidence;
   correct prior exact-parity/zero-clipping/all-V1-closed/clean-tree claims.
   Update UI_DESIGN_BRIEF, MCU_BASELINE (obsolete figures marked historical),
   user guide, PLAN, DEV_LOG and indexes. Do not scatter handoffs.
8. Keep CURRENT as complete spec; mark IMPLEMENTED — awaiting review only when
   scope finished. List all genuine skipped/blocked checks together, attempts
   and reasons. No zero-bug or physical LCD/MCU/RF/touch claims without testing.
