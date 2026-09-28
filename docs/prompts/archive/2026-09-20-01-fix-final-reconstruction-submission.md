> ARCHIVED — historical context only. Active handoff: [CURRENT](../CURRENT.md).
> Preserved before the independent 2026-09-20 icon/color review.

# CURRENT — finish Quiet Architecture faithfully and close remaining defects

Date: 2026-09-18. Status: **IMPLEMENTED — awaiting review**.
ONE implementation/fix delivery: Q1–Q5 below, including self-review and regression.
Owner requests resemblance to the seven approved designs and regenerated icons.
Review/technical-lead findings and implementation guidance below are not a newly
owner-approved visual design. No unresolved product choice is needed for this work.

## Read first and inspect evidence

Read AGENTS.md, docs/prompts/README.md, docs/README.md,
docs/HARDWARE_OVERVIEW.md, docs/MCU_BASELINE.md, docs/UI_DESIGN_BRIEF.md,
docs/user/SMART_HUB_UI_GUIDE.md, docs/user/LORA_GUIDE.md and LORA_PROTOCOL.md.
Read [latest review](../../reviews/2026-09-18-reconstruction-review/REVIEW.md),
[visual evidence](../../reviews/2026-09-18-reconstruction-review/COMPARE.html),
and review-only probe.c / PROBE.txt in that folder.

VIEW all seven ORIGINAL files in docs/drafts/2026-09-17-pro-max/:
dashboard-quiet-architecture.png, trends-quiet-architecture.png,
devices-quiet-architecture.png, settings-quiet-architecture.png,
auto-co2-quiet-architecture.png, auto-humidity-quiet-architecture.png,
auto-voc-invalid-quiet-architecture.png.
These remain visual authority; native submissions/reports are NOT substitutes.
Use ui-ux-pro-max if available; reconstruct, do not invent another design direction.

Inspect full working diff and snapshot/hash starting sources. Preserve unrelated
dirty changes. No commit/push/branch/reset/clean/stash, new drivers/Flash/CRC,
Node automation, packet changes or features. Review-only probe is evidence, not
production code. Fix all known issues together, not owner-mediated phases.

## Keep accepted work; do not redo it

- Two-row header/navigation and current major page proportions; horizontal
  environment cards; full-page focused editors releasing background widgets.
- Measured Home unit positioning, readable1420/10000 and signed-temperature
  handling;80px Auto live strip;44px targets; stable invalid field outlines.
- Paused/offline ordinary rows;1000-hour header fit; truthful readback switches.
- Improved cylindrical purifier, four-blade fan, clean drop, connected desk lamp.
  Refine optical balance where needed, but do not regenerate the entire family
  without reason. Wire unused proper control assets instead of textual substitutes.
- Exact ZIP membership/hash/duplicate negative tests; actual clock rollover
  CHECK1; CHECK28 genuine second PENDING/latch setup; accepted engine/integration.
- Production verification passes outside sandbox. Do not “fix” sandbox permission
  failures by removing watchdog checks. Remaining assertions below are separate.

## Fixed layout/typography translation

These are existing technical-lead 800x480 anchors, not pixel-exact raster CAD.
Small4–8px fitting adjustments allowed with explicit reason; keep major layout.

| Region | Preserve |
|---|---|
| Header | x24..776,y0..56; Studio/home left, uptime/link right |
| Tabs | separate y56..100; selected celadon capsule only |
| Main | x24,y112,w752,h344 |
| Overview | left416 +gap16 +right320; hero416x216; two200x112 tiles below, gap16 |
| Devices | 2×2 cards368x164; gaps16 |
| Trends | rail184 +gap16 +chart552 |
| Settings | context title; form x24,y112,752x280; actions y408,h48 |
| Auto | strip x24,y108,752x80; two368x148 cards y200; helper y356,h52; actions y416,h48 |

All visible text >=18px including fallbacks, errors, axes, disabled and dropdown.
18px is a MINIMUM, not default for every role. Studio approximately34, tabs24;
primary page/card headings and readings stronger than labels and units.
Account for actual line metrics (font34 line46; font48 line65).
Touch targets >=44px; steppers/footer >=48px; independent action hit regions
separated by8px. Do not solve collisions by shrinking below18 or obscuring data.

## Q1 — repair boundary and recovery layouts

Reproduce against current sources BEFORE editing using supplied probe/captures:

1. Trends CO2 live/history10000: Current/Min/Max strings collide.
   Use distinct number and smaller-unit labels/groups with measured bounds,
   baselines and reserved gutters; no hardcoded suffix x-offsets. Include full
   supported readings, unavailable values, negative temperature and wire-end
   robustness. Size axis gutter/unit region by actual maximum text width.
   Preserve numeric truth; no ellipsis/clipping/clamping solely to fit.
2. Genuine Auto failure → timeout/latch → late ACK/reconnect yields IDLE with
   latch retained. BOTH Overview and Devices move the switch left into name.
   Allocate state-dependent name/mode/control lanes so switch + Retry stay
   simultaneously legible and independently tappable. Concise display label
   “Ventilation” allowed; preserve stored config/identity. Keep normal layout.
   Verify all presets, both polarities, Sending/Unknown/offline/sensor pause,
   recovered latch and Manual override. No overlap with glyphs, not only hitboxes.
3. Settings Fan ON10000/OFF9950: summary overlaps chevron. Reserve chevron lane;
   use readable compact two-line ON/OFF layout or measured smaller unit, never
   remove association or truncate values. Also inverse RH and non-step imports.
4. Pending-command polarity/preset change: rejected Save currently draws error
   over Output polarity. Keep commit guard and intact draft. Use a dedicated
   visible error lane/inline row message with stable form/actions; no text on
   control captions, no overlaid toast covering form. Verify recovery and valid
   subsequent Save/Cancel. Invalid-threshold Back already works; preserve it.

Promote these exact native reproductions into automated regression assertions.
Do not merely make screenshots of nominal420/85 again.

## Q2 — truthful Trends timing

Current ui.c duration_min uses valid_cnt*5 and cap15; two one-minute samples
at23:59/00:00 show Last5min and labels out to00:14. Fix from real stored timing,
not number of valid points. Never fabricate samples to match the concept.

Technical-lead guidance: show recorded coverage using actual oldest/newest sample
timestamps with midnight-safe delta, omit duration for zero/one samples or use a
truthful single-reading label. If keeping fixed16-slot plotting, clearly separate
its domain/window from recorded coverage and do not call future empty slots
history. Alternatively bound axis to present samples without inventing data.
Maintain correct point-to-timestamp mapping, gaps and inspection after shift.

Tests: empty, singleton,2/4/16 samples, invalid gaps, sliding20→16,
midnight23:59→00:00, all-invalid, live unavailable with retained history.
Current/Min/Max/selector validity must agree with their respective sources.

## Q3 — finish already-requested visual hierarchy and assets

- Match original hierarchy, not all-small text: Studio34/tabs24, stronger
  Devices heading and Settings-row caption, prominent readings and subordinate
  units. Keep accepted overall card geometry. Restore calmer number/label rhythm.
- Hero: visible celadon check/status badge for Good and appropriate non-misleading
  status treatment for other categories, wind icon, vertical metric separator.
  Keep category thresholds unchanged. Replace broad “Indoor air parameters
  optimal” with scope-neutral copy such as “Based on CO₂ and VOC”; never imply
  missing temperature/humidity or all air properties were assessed.
- Remove hidden CO2/VOC quality-label widgets retained solely for tests.
  Test visible aggregate semantics or pure category model, not hidden UI.
- Trends selectors: sensor icon/title with prominent readings and smaller units;
  current stats same hierarchy. Add restrained area shading, stronger forest
  line and endpoint/inspected-point emphasis instead of equal6px dots everywhere.
  Do not smooth into invented measurements or bridge missing samples.
- Devices/Home Auto: show reference leaf + sensor association when Auto eligible;
  pause/error copy remains explicit. Keep Manual production defaults.
- Actually use generated coherent chevrons, plus/minus and dropdown icon.
  Settings Back must not remain literal '<'; Auto '+' / em dash must not
  replace available vector steppers. Size by visible ink, not asset canvas.
  Sensor/control icons should have comparable optical weight per role.
- Retain warm stone/porcelain, dark ink, celadon. Restore restrained reference
  tonal depth on hero/primary controls with bounded LVGL rendering if feasible.
  No framebuffer/background screenshot, heavy blur or large gradient cache.
  If measurable heap/driver limitation blocks an effect, document the tested
  low-cost approximation explicitly; do not claim an omitted effect is present.
  Pixel-perfect photographic texture is not required.

Do not reintroduce VOC Index/ppb, wall-clock header, Details or notification panel.
Do not blindly copy concept small typography. Existing valid/error Auto helper
slots and buttons must remain stable after visual changes.

## Q4 — close remaining proof gaps, not add unrelated infrastructure

- Replace simplistic audit_visible_glyph_containment: it checks only label y
  against immediate non-scrollable parent. Check actual visible ink/effective
  clipping in x/y through relevant ancestors and known sibling content/control
  lanes. Account for font metrics, overflow/scroll semantics; intentional clipped
  offscreen dropdown entries are not errors. Audit fully visible selected rows.
- Add negative controls rejecting horizontal stat collision, switch over name,
  summary under chevron and Settings error over polarity, plus an ancestor-clip
  case. Keep >=18px audit and its negative control.
- CHECK31: exact counters are good but phase==IDLE does not prove sent command.
  For each of all five scheduled lifecycles, assert sequence transition,
  recorded TX bytes/target mask and resulting readback for BOTH directions.
  Distinguish real Pending, timeout/latch, late recovery and explicit re-arm.
  Verify an omitted/no-op transition cannot pass. Keep50 cycles, >4096 free,
  abs(cycle10→50 drift)<=256, fragmentation/largest-block diagnostic only.
- Watchdogs: track owned worker AND descendant PIDs while alive; verify bounded
  death of every worker/child for both single and concurrent helpers, not only
  parent cmd PID extracted from exception. Surface cleanup/taskkill/wait failures;
  retain output/error/exit diagnostics and sentinel hashes. Scope termination
  to owned processes (avoid PID-reuse hazards), no global killing.
  Disposable canonical-path-checked fixtures only, retain failed diagnostics.
- tools/build_review_pack.py must return nonzero if its regression subprocess
  fails; add controlled failure test and bound subprocess runtime. Retain evidence.
  Do not let report generation imply checks ran/passed just because assets exist.

Preserve real passing ZIP/race/sentinel tests and clock/Retry matrix.
Do not weaken accepted safety checks or mutate production solely to satisfy tests.

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

## Q5 — evidence, documentation and final handoff (same delivery)

After final relevant edit, run:
- tools/build_sim.bat
- tools/run_regression.bat
- tools/export_mcu.bat --verify
- tools/verify_production.bat

Keep isolated production C99 build without UI_TEST_HOOKS/include-simple macro;
negative executable must run, exit1 and emit expected stderr. Record permission
limitations explicitly, retry safe alternatives before declaring code defective.

Capture actual800x480:
- Seven original pages with matched TEST fixtures: Auto on Purifier/Fan in
  Overview/Devices, Fan Auto in Settings, live420 in CO2 editor, RH48,
  invalidVOC150/150. Do not modify production defaults.
- All four Trends metrics, Q2 timing/gap matrix and Q1 boundary/recovery screens.
- Header100/1000h Connected/Disconnected, unsupportedManual-only, preset dropdown
  top/bottom, min/max/non-step editor values and invalid Back/Cancel/Save flows.

Inspect every final capture yourself and provide same-size reference/native
comparison and native icon contact sheet. Add actual/target typography and
geometry table with justified differences; do not label everything MATCH by default.
Use meaningful crops/overlays, not whole-image similarity scores as safety proof.

One report under a NEW docs/reviews/<date>-<descriptive-delivery>/; keep previous
review evidence intact. Include full final logs, source/executable hashes,
real immediate/settled heap states, redraw/dirty metrics, exact passes/skips.
Close Q1–Q5 with evidence, not statements. All in-scope regressions repaired in
this run; no staged owner-mediated prompts.

Correct active README/indexes, UI_DESIGN_BRIEF, user guide, MCU_BASELINE, PLAN,
DEV_LOG and prior submission claims using explicit supersession notes.
- Host object size != MCU linked Flash/RAM or total firmware usage. Identify
  toolchain/build flags/test hooks; no promised remaining1/2MB flash without
  target map, LVGL/firmware and buffer accounting.
- CHECK31 minimum at Start is not transient-editor minimum. Report actual
  measurement context and lower CHECK8 Home value; +8bytes is bounded observed
  drift, not proof of zero leaks. Do not mix maxima from different samples.
- Mark obsolete Study12/four-font/~39KB statements historical, not current.
- Report actual screenshots: old offline capture is00:00:03, not1000h.
  Do not claim area fill, all-ancestor audit, descendant death or icon use
  without implementation/evidence.

CURRENT remains complete active spec. Mark IMPLEMENTED — awaiting review only
when finished; visual acceptance belongs to owner/reviewer. Report all genuine
blockers/skipped tests together. LCD optics/touch/QSPI/RF/MCU timing and target
memory map remain untested until physical validation. No “zero bugs” guarantee.
