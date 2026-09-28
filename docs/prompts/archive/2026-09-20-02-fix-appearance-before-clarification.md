> ARCHIVED — historical context only. Active handoff: [CURRENT](../CURRENT.md).
> Preserved the submitted IMPLEMENTED status before the owner's clarification
> about icon appearance and simulator-only color comparison.

# CURRENT — finish Quiet Architecture icons, RGB565 fidelity and remaining regressions

Date: 2026-09-20. Status: **IMPLEMENTED — awaiting review**.
ONE consolidated implementation/fix delivery: C1–C6, full self-review and final
verification together. Do not split into phases/prompts or send each issue back
to the owner for a separate review.

Owner request: review the changed project and explain/fix why icons and colors
still differ from the accepted Quiet Architecture concepts. The owner has not
approved a new palette/design. Canonical-source choices and numeric optical/color
targets below are technical-lead translation guidance, not new product decisions.

## Read first

Read AGENTS.md, docs/prompts/README.md, docs/README.md, docs/HARDWARE_OVERVIEW.md,
docs/MCU_BASELINE.md, docs/UI_DESIGN_BRIEF.md, docs/user/SMART_HUB_UI_GUIDE.md,
docs/user/LORA_GUIDE.md and LORA_PROTOCOL.md.
Read [independent review](../../reviews/2026-09-20-icon-color-review/REVIEW.md),
[three-way comparison](../../reviews/2026-09-20-icon-color-review/COMPARE.html),
MEASUREMENTS.json and review-only probe.c/PROBE.txt in that folder.
Use the repo-local ui-ux-pro-max skill if available. Do not redesign from scratch.

VIEW all seven ORIGINAL images under docs/drafts/2026-09-17-pro-max/:
dashboard-quiet-architecture.png, trends-quiet-architecture.png,
devices-quiet-architecture.png, settings-quiet-architecture.png,
auto-co2-quiet-architecture.png, auto-humidity-quiet-architecture.png,
auto-voc-invalid-quiet-architecture.png.
Originals are raster concepts, not consistent vector assets. Submitted native
captures, the historical browser prototype and author MATCH claims are not
replacement visual authority.

Inspect the full dirty diff, snapshot/hash starting sources, preserve unrelated
owner changes. No commit/push/branch/reset/clean/stash, new drivers/Flash/CRC,
Node automation, packet changes, new modes/features or wholesale UI rewrite.
Review diagnostic code is evidence, not production code.

## Preserve accepted work and stable contract

Keep current two-row shell, page compositions, Plus Jakarta Sans font family,
focused editors that release background widgets, truthful readback switches,
ordinary/pending/offline/paused states and tested Auto engine behavior.
Retain repaired 10000ppm stats, 10000/9950 Settings summary, pending-polarity Save
error lane and late-ACK switch/Retry lanes. Do not reopen them as new defects.
Two consecutive midnight samples now label correctly; preserve that repair.
Home ellipsis during recovered-latch Retry is not a collision; keep identity
accessible in Settings. No need to redesign all appliance rows again.

Geometry anchors at800x480:
- Header x24..776,y0..56; tabs y56..100. Main x24,y112,w752,h344.
- Overview left416 +gap16 +right320. Hero416x216; lower tiles200x112,gap16.
- Devices2x2 cards368x164,gap16. Trends rail184 +gap16 +chart552.
- Settings retains current shortened form and dedicated error lane; actions
  y408,h48. Do not restore a taller form over the repaired error slot.
- Auto strip752x80,y108; cards368x148,y200; helpery356,h52;
  actionsy416,h48. Error/helper/actions stable between valid and invalid drafts.
Small4–8px fitting changes are allowed with measured reason, not a new composition.
All visible text >=18px, including error/disabled/axis/dropdown/fallback glyphs.
Touch>=44px; steppers/footer>=48px; independent targets have>=8px separation.
Optical icon extent may grow inside existing content lanes without growing cards.

## C1 — make icon resemblance deterministic

Do NOT run another unconstrained AI image generation or replace the family with
generic library icons. Correct/trace canonical SVG geometry once and reuse it.
Keep source SVGs under one documented assets directory (for example
assets/icons/quiet-architecture/), with one manifest of semantic role, source
reference/crop, viewBox, optical bounds, output size and stroke per role.

Resolve original inconsistencies explicitly:
- Devices image is canonical for purifier/fan/humidifier/desk-light silhouettes.
  In particular, use its one-row perforated purifier everywhere, not the
  three-row variant from the VOC concept. This is consistency guidance from
  technical lead, not a claim that both originals were identical.
- Dashboard is canonical for home/timer/air/status; Settings/Auto for controls.
- Reuse the same geometry in different sizes. Normalize by visible ink and
  silhouette proportions, NOT just width/height of the bitmap canvas.

Measured Devices ink extents at800x480:
purifier~36x51, fan~42x49, lamp~36x50, versus current24x34,34x34,34x35.
Aim within roughly2px of these optical extents with coherent2–3px effective
strokes; preserve the reference's non-square proportions. Reference crops use
dark-pixel thresholds, so tolerate AA edge differences rather than demanding
identical binary masks. Measure drop, home, timer, air and control roles likewise.
Home's smaller appliance icons must follow Dashboard role scale, not blindly use
the Devices dimensions. Keep gutters to text, switches and Settings actions.

Use near-black ink for main appliance/sensor icons on light surfaces, not forest
green. Semantic status/selected icons may retain appropriate inverse/celadon
colors. Foreground is a theme role, not permanently baked into a color bitmap.

Generate at final role sizes with a pinned build-time rasterizer; evaluate4x
supersampling/downsampling for AA. Keep runtime LVGL A4 masks unless a measured
A4/A8 comparison justifies selective A8. Do not solve shape problems with A8.
No runtime SVG parser, full-background bitmap, screenshot-cropped rectangle or
per-frame scaling. Inspect compiled native A4 masks, not only browser SVG previews.
Add deterministic regeneration/check mode and source/output hashes; existence
must not be reported as verification. Preserve static storage/include conventions.
Pin Plus Jakarta font/weight and dependencies; fail clearly if required inputs
are missing rather than silently substituting Segoe. Record font source/license.

## C2 — correct palette based on RGB565 output, not guesswork

Do not change every color or globally increase saturation. Current canvas and
celadon are close. Use measured original Dashboard blank-region medians as
guidance, not universal flat tokens for every textured pixel:
canvas#EEECE7, surface#F3F1EC, upperhero#23443B, lowerhero#19372F,
celadon#D7E0BC. Current decoded colors are#EFEBE7,#F7F7EF,#183829,#183829,#D6DFBD.

RGB565 quantization is real but does not require a flat, darker/less-blue hero.
Current#1D3B2E truncates to displayed#183829. Test representable upper/lower
candidates such as#214539/#183831; select final tokens after native comparison.
Keep near-current canvas/celadon, reduce the surface's excess brightness/
yellow-green cast where measured, and match near-black ink. Preserve contrast.

Restore restrained hero/primary-control depth using bounded LVGL rendering.
A simple two-stop gradient is preferable to photographic texture. Inspect actual
local LVGL8.4 gradient allocation/cache behavior before implementation.
Keep42KiB heap, no framebuffer, no heavy blur/compositing, no blanket enabling
of a large gradient cache. Measure transient and settled memory/dirty regions.
If a tested effect genuinely cannot meet constraints, document the measured
limitation and low-cost approximation; do not claim an omitted gradient exists.
Texture/exact AI-rendered lighting are non-blocking; arbitrary flat recoloring
is not a substitute for evaluating the specified low-cost depth.

Keep the splash/logo background bit-exact with final canvas so no rectangle seam
appears. Apply consistent palette roles across all seven pages and states.
Document source RGB888, encoded565 and decoded display RGB888 in one token table.
Do not change RGB/BGR, byte swapping, gamma or LCD drivers without target evidence.

## C3 — repair Trends selector state contrast

update_trends_tabs() changes icons/names/numbers but omits s_trends_tab_units.
After selecting Temperature/Humidity, CO2 ppm stays white on porcelain (~1.08:1);
selected units can remain muted on forest (~2.10:1).
Update number/unit/icon/title colors together in selected and deselected states.
Test all four metrics in both directions and after page recreation, offline,
invalid and restored data. Require >=4.5:1 for these18px units on actual decoded
backgrounds, not just nominal RGB888 tokens. No shrinking or hiding valid units.

## C4 — repair plot domain, gaps, singletons and emphasis

Reproduce independent probe_trends_edge_gaps and probe_trends_middle_singleton.
For16 timestamped slots10:00..10:15 with indices2..13 valid, the chart positions
over16 slots while axis labels use10:02..10:13. Labels and point/tooltip timestamps
must share ONE domain. Keep invalid slots/gaps; do not compress time silently.
Prefer retaining stored-slot domain and computing corresponding ticks from its
timestamps; distinguish valid-data coverage from plot-domain coverage in copy.
Do not call stale retained readings “Last N min” as if they end now.

Draw every valid isolated sample, including the sole sample in the middle of
an otherwise invalid buffer. No fabricated points/lines across missing data.
Inspect LVGL8.4 line draw callbacks in src/extra/widgets/chart/lv_chart.c:
p1/p2 and rect_dsc can coexist. Current line/else-if marker logic misses regular
markers; stock rendering can omit a point beside a missing next point.
Handle drawing/marker state independently and reset descriptors as necessary.
Highlight last VALID/inspected point, not merely history_count-1.
Avoid repeated translucent shared-edge bands under adjacent segments.
Do not spline/smooth into invented telemetry.

Tests/assertions, not only object containment:
empty, singleton atfirst/middle/last index,2/4/16 samples, leading/trailing/
interior invalid gaps, all-invalid,20→16 sliding, midnight, live unavailable
with retained history, correct tooltip mapping and inspection after shift.
Keep numeric truth, range robustness and all four metric units intact.

## C5 — complete existing type hierarchy without breaking layouts

Current nav labels use18, despite prior24px target/report. Finish that existing
role target with fitted tab widths/gutters;18px remains the floor elsewhere.
Studio stays34. Restore larger nominal hero readings (approximately48 where
they fit), with measured34 or other>=18 long-value fallback, and subordinate units.
Auto reading/threshold groups need separate smaller units with aligned baselines;
they currently concatenate number and unit in34px. Avoid hardcoded suffix offsets.
Do not globally enlarge every caption or return to the old clipped48px stats.
Verify420,10000,0/500VOC, signed temperature,100%RH, unavailable and non-step
imported values. The approved layout and repaired maximum-value rows take priority
over copying small concept text or photographic details.

## C6 — targeted proof, safe verification and honest reporting

Keep passing33 checks and production/export negative tests. Extend them to fail
on C3/C4 with assertions for role colors, point visibility and actual axis mapping.
Font/object containment cannot certify semantics or actual glyph ink; retain
negative controls and add focused label-ink/known-lane checks where needed.

Keep watchdog owned-worker safety: numeric PID alone is not identity after exit.
Track/verify process identity or owned handles, including descendants; never kill
a reused unrelated PID. Surface enumeration failures rather than claiming no
children. Bound stream draining and exception/timeout paths, not just root waits.
Test early parent exit/child retaining pipes and both single/concurrent helpers
with disposable canonical-path-checked fixtures. Preserve diagnostics/sentinels;
do not weaken checks or globally kill processes. No unrelated tool rewrite.

New evidence folder under docs/reviews/<date>-<descriptive-delivery>/.
Do not overwrite historical packs. Make contact sheet from compiled masks; provide
canonical SVG vs actual native crops at1x/4x with measured optical bounds.
Capture all seven pages with matched TEST fixtures (Purifier/Fan Auto for
Overview/Devices, Fan Auto Settings, live420CO2,48RH, invalidVOC150/150).
Never change production defaults to match screenshots. Include C3/C4 repros,
all metric selection states, repaired Q1 boundary/recovery screens, offline,
pending, Retry, dropdown top/bottom,100/1000h headers and invalid editor flows.

After the LAST relevant edit run:
- tools/build_sim.bat
- tools/run_regression.bat
- tools/export_mcu.bat --verify
- tools/verify_production.bat

Inspect the final captures and full diff yourself; fix in-scope regressions in
this SAME delivery. Report exact passes/skips and permission limitations.
Keep full final logs, source/font/asset/executable hashes, typography/geometry
table, actual565 palette table, immediate/settled heap and dirty-region metrics.
No whole-image similarity score as safety proof; no default “all MATCH.”

Update active README/indexes/UI_DESIGN_BRIEF/user guide/MCU_BASELINE/PLAN/DEV_LOG.
Explicitly supersede inaccurate historical claims without deleting evidence:
46 icons, not40; current host objects icons21120B/fonts100336B at review time;
Plus Jakarta, not Segoe; nav18 until actually changed; no current hero gradient;
old Study12/four-font/~39KB statements are historical.
Host object sizes are NOT MCU linked Flash/RAM or firmware remaining space.
Pixel-for-pixel restoration is not proven by author assertion or33/33.
CURRENT remains sole active prompt; mark IMPLEMENTED — awaiting review only
after completion. Visual acceptance belongs to owner/reviewer.

## Frozen functional/embedded contract

LVGL8.4,800x480 RGB565,42KiB heap, ONE800x10 partial draw buffer16,000B.
No MCU framebuffer, per-tick allocation/float/trig or ISR LVGL calls.
Static captions use process-lifetime const storage/lv_label_set_text_static;
dynamic labels use fixed buffers; change-guarded redraw stays bounded.
Keep page-widget release while editors are open and preserve model/Auto/history.

ui.c owns six globals exactly once: lora_hub_cmd,lora_node_status,
lora_last_rx_tick_ms,lora_rx_revision,lora_last_snr,lora_last_rssi.
Headers extern; plain #include "lvgl.h";8-byte command/20-byte status unchanged;
Hub semantics/polarity/Auto, Node raw GPIO; ui_is_tx_ready gating unchanged.
Uptime,Connected/Disconnected,signal bars,splash,English,°C,session-only history.
No Details/notification panel,VOC Index/ppb,new health guarantee or AQI formula.
Readback switches, not optimistic target state.

Auto rules:
Fan/CO2 ON>=1000 OFF<=800, bounds0..10000,step50,ON>=OFF+50.
Purifier/VOC ON>=150 OFF<=100,bounds1..500,step5,ON>=OFF+5.
Humidifier/RH ON<=40 OFF>=50,bounds0..100,step1,OFF>=ON+5.
Valid imported integers need not be step multiples. Preset changes load defaults;
unsupportedManual-only; missing/invalid/legacy configManual; explicit validAuto.
Three eligible fresh frames plus10s hold after enable,committed limit change,
confirmed transition,boot/reconnect,sensor recovery.
No Auto decision offline/invalid/pending/latched; no forcedOFF on loss.
Manual override has no timed return. Retry or deliberateManual→Auto re-arms;
ordinary edit/reconnect/lateACK does not. No-op Save no callback/reset.
BackAuto preserves shared draft; BackSettings/Cancel discards; Save commits once;
Reset limits edits draft thresholds only. No new persistence/CRC guarantee.

Keep50-cycle heap stress, free>4096B and abs(cycle10→50 drift)<=256B;
report actual sampled states, not a claim of all-time peak or zero leaks.
Target LCD optics/touch/QSPI/RF/timing and linked MCU memory remain untested.
