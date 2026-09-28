> ARCHIVED — historical context only. Active handoff: [CURRENT](../CURRENT.md).
> Preserves the provenance-based icon recovery handoff.

# CURRENT — recover Quiet Architecture icon appearance from the actual design pixels

Date: 2026-09-20. Status: **IMPLEMENTED — awaiting review**.
Implementation delivered in [icon recovery review pack](../reviews/2026-09-20-icon-recovery-delivery/REPORT.md).
ONE focused implementation/fix delivery, self-review and final regression together.
Owner rejects icon appearance after repeated redraws and asks for instructions
grounded in how the original design was made. This is NOT a resizing request,
new design direction, palette redesign or physical LCD calibration task.

The last agent claimed C1–C6 complete. This targeted provenance review confirms
remaining icon/pipeline defects, not the status of every intervening runtime fix.
Preserve the current Auto, Trends, color and type repairs. Reproduce before
changing anything else; do not replay historical defects blindly.

## Read and inspect first

Read AGENTS.md, docs/prompts/README.md, docs/README.md, docs/HARDWARE_OVERVIEW.md,
docs/MCU_BASELINE.md, docs/UI_DESIGN_BRIEF.md and relevant user/wire guides.
Read [provenance/appearance review](../reviews/2026-09-20-icon-origin-review/REVIEW.md),
[actual A4 comparison](../reviews/2026-09-20-icon-origin-review/COMPARE.html) and
[SOURCE_CROPS.json](../reviews/2026-09-20-icon-origin-review/SOURCE_CROPS.json).
Use ui-ux-pro-max as requested by owner, but do not use a generic icon-library
recommendation to replace the actual original shapes.

Read original DESIGN prompts only as provenance:
- archive/2026-09-17-04-design-quiet-architecture.md
- archive/2026-09-17-05-design-quiet-companions.md
- archive/2026-09-17-06-design-quiet-settings-auto.md

VIEW the seven accepted PNGs in docs/drafts/2026-09-17-pro-max/.
Do not use later native screenshots or the historical browser prototype as the
source for shape reconstruction. Snapshot/hash the current dirty work first.
Preserve unrelated edits. No commit/push/reset/clean/stash, new drivers, new
features, Node automation, Flash/CRC or protocol changes.

## Provenance correction — stop inventing an original vector library

ui-ux-pro-max supplied design guidance. The approved images were made by image_gen:
Dashboard from a text prompt without input image; Trends/Devices using Dashboard
as reference; Settings/CO2 using Devices; RH/VOC via editing the CO2 raster.
The original prompt requested consistent outline icons but specified NO Phosphor,
Lucide, Heroicons, asset URL, glyph ID or SVG path.

Therefore there is no evidenced original vector set to fetch from the skill.
Existing assets/icons/quiet-architecture/*.svg are later approximations, not
original design assets. Do not repeat claims of recovered exact SVG paths.
The accepted raster pixels are the surviving authoritative appearance source.

Earlier technical-lead SVG-only reconstruction guidance is superseded here:
use source-derived transparent alpha masks as the PRIMARY build-time source.
LVGL already consumes raster A4 masks; handwritten SVG is not a prerequisite.
Do not generatively redraw icons or select semantically similar library symbols.

## A — extract the exact source shapes, not another interpretation

Use original-size PNG coordinates, right/bottom exclusive, as recorded in
SOURCE_CROPS.json. Check image dimensions1619x971 and source hashes; visually
verify the crop before extraction. Do not use800x480 coordinates on1619x971 files.

| Icon | PNG stem (all end in -quiet-architecture.png) | Source ROI [l,t,r,b) |
|---|---|---|
| Purifier | devices | [100,255,190,377] |
| Fan | devices | [852,252,957,374] |
| Drop | devices | [102,606,186,716] |
| Lamp | devices | [855,601,944,723] |
| Home | dashboard | [55,34,120,99] |
| Timer | dashboard | [1115,50,1157,95] |
| Thermometer | dashboard | [104,717,151,799] |
| Sliders | settings | [102,405,180,487] |
| Leaf | settings | [102,549,177,624] |
| Gear | settings | [98,685,180,766] |
| Back | settings | [63,139,103,197] |
| Forward | settings | [1483,558,1520,610] |
| Dropdown | settings | [1456,282,1505,322] |
| Minus | auto-co2 | [128,626,184,653] |
| Plus | auto-co2 | [671,611,729,669] |
| Warning | auto-voc-invalid | [48,746,111,807] |
| Air streams (light-on-dark) | dashboard | [760,251,858,342] |
| Check badge (light-on-dark) | dashboard | [399,331,486,415] |

Devices is the already-selected canonical source for appliance silhouettes on
all pages. Its one-row purifier wins over the inconsistent three-row VOC concept.
This is a technical-lead consistency choice, not proof the originals are identical.
Heater/socket/unused navigation symbols without a genuine reference are documented
extensions: preserve working symbols and never fabricate extraction provenance.
Keep live signal bars and state behavior intact; no static screenshot substitution.

Build-time extraction requirements:
1. Preserve the original crop separately as evidence; store an8-bit grayscale
   alpha master at source resolution under one assets/icons/quiet-architecture
   source folder. This is icon-only alpha, NOT a colored screenshot rectangle.
2. Remove the warm surface from BOTH exterior and interior holes. Estimate the
   local background B(x,y) from clean margin/background pixels, using a smooth
   plane if needed for the source's subtle gradient. Estimate foreground F from
   opaque core pixels, not a hardcoded pure black or all-image minimum.
3. For single-ink regions use the compositing model C = alpha*F+(1-alpha)*B.
   A useful bounded estimate is:
   alpha = clamp(dot(C-B,F-B)/dot(F-B,F-B),0,1).
   Record whether fitting operates in encoded sRGB or linear light; choose by
   reconstructed-crop residual, don't mix spaces silently. This is an estimate,
   not recovery of hidden original alpha. The originals have rendered shading.
4. Use separate light-on-dark and red-warning foreground/background estimates.
   Do not apply dark-pixel thresholding to air/check or invert an entire crop.
   For the check badge, the dark check must remain a transparent cutout showing
   the hero behind it, or an explicitly separated second mask if needed.
5. Preserve filled/hollow topology: the Home door is FILLED rectangular ink;
   fan hub and blade interiors are hollow; lamp base interior is hollow.
   Preserve all five purifier perforations and their observed centers/curvature.
   Do not erase small legitimate components during denoising. No automatic
   dilation/erosion, thickening, symmetrizing or replacing curves with circles.
6. Keep AA edge coverage. Inspect for background halos on light AND forest
   backgrounds. Correct background estimation/support locally; don't hide bad
   extraction by hard binary thresholding or globally thickening the silhouette.
7. Source masks are canonical. If a vector source is still desired, fit outer AND
   inner contours to the extracted mask, including all holes/filled regions.
   It must pass the same contour comparison; semantic hand-written24x24 paths
   are no longer acceptable substitutes. Do not invent Bézier coordinates.

## B — specific appearance mistakes to eliminate

These are current-source observations, not optional alternate styling:
- Home: current path uses an arched hollow doorway ('a2.5 2.5'). Original doorway
  has a flat lintel and rectangular filled dark area. Restore it from pixels.
- Fan: current blade apex/control points create pointed ends/cusps and narrow
  interior holes. Original blades have smooth rounded paddle ends and larger
  open interiors around the hollow hub. Do not just change overall width/height.
- Lamp: original shade has a rounded crown and asymmetric taper, not the current
  sharp triangular wedge. Original base has flat bottom/rounded upper shoulders,
  not a capsule. Preserve curved shoulder between stem/arm and original joint.
- Purifier: rim, top button, walls and lower slit are too heavy; vents are forced
  onto a flat row instead of following the observed slight curvature.
  Reference sidewall sample is5/104 of ink height versus current4/55 (~1.5x
  normalized weight). This is diagnostic, not a universal fixed stroke value.
- Drop: current broad shoulders/abrupt round bulb differ from source contour.
- For sliders, leaf, thermometer, gear, timer and controls, use the corresponding
  crop rather than assuming the existing generic path matches. For gear, trace
  the actual teeth; do not describe a guessed tooth count as authoritative.

Keep existing display boxes, page positions and hit areas. Fit canonical masks
uniformly with transparent padding; preserve aspect ratio. Change bounds only
if unavoidable to prevent clipping, and document it. This is not an enlargement
pass. Ensure complete outlines remain inside bitmap storage, especially current
lamp/purifier extents touching bitmap edges. Do not stretch axes independently.
Use existing theme role colors; do not introduce a global palette change.

Generate each runtime size DIRECTLY from the high-resolution alpha master using
one documented area/Lanczos downsampling step. No upsample-lowres-then-downsample.
Quantize alpha to A4 with specified rounding and row padding; emit LVGL8.4
LV_IMG_CF_ALPHA_4BIT descriptors, w/h/data_size consistent. Recolor at runtime.
No runtime SVG parsing, background screenshot, per-frame scaling or new heap cache.
Use A8 only selectively if an actual A4 comparison demonstrates loss of a required
detail; record ROM cost. A8 cannot fix wrong contours or filled/hollow topology.

## C — one real source-to-bitmap pipeline

Current generate_assets.py reads its own SVG_PATHS dictionary and does not load
the exported SVG/manifest. create_icon_manifest.py owns another independent
dictionary. Fix this within the same delivery:
- One consumed manifest links original image SHA256, ROI, extraction settings,
  alpha-master hash, optional contour-fitted vector hash, each runtime role/size,
  padding, color role and emitted descriptor/hash.
- Generator loads those masters/manifest; no shadow hardcoded icon dictionaries.
  Retire duplicate reconstruction paths without deleting owner evidence.
- Dedicated icon generation/check path must not regenerate fonts/splash or change
  palette accidentally. Keep plain #include "lvgl.h" and static const storage.
- Default 'exist and verified' must become real input/output verification or an
  honest skip. Provide non-mutating --check, nonzero for stale/missing/mismatched
  canonical source or generated output. Test a changed source and tampered output
  in disposable scoped fixtures, not by corrupting owner production assets.
- Check all generated descriptors and transparent margins. Source hash matches
  prove reproducibility, not resemblance; keep both checks separate.

## D — visual acceptance with actual source and actual emitted pixels

Use the current source comparison as a failing baseline. The new evidence MUST
decode emitted ui_icons.c A4 arrays, then also capture actual simulator rendering.
tools/build_review_pack.py currently re-renders SVG for its contact sheet;
replace that proof path with the generated mask decoder. Keep original crop,
canonical alpha master, generated mask and final native capture distinct.

For each referenced icon provide:
- Exact original crop, isolated alpha master, generated A4 at native size and4x.
- Equal-ink-height comparison with aspect preserved and matching center alignment.
  No neighboring text/leaf pixels, independent X/Y stretching or simulated
  “native” SVG preview. State alignment transform/thresholds.
- Outer/inner contour overlay and feature checklist: door fill, fan tips/hub holes,
  shade/base shape, purifier holes/button/slit, droplet shoulder, control details.
- Numeric contour diagnostics at the same pixel grid (e.g. bidirectional boundary
  distance and coverage difference), reporting edge threshold/AA tolerance.
  Do not invent a universal pass percentage or force binary IoU100% for an AA image.
  The topology/details above must be correct regardless of aggregate score.
- Recompose the alpha master onto estimated source background to detect residual
  halos/contour loss; then show recoloring on real light/dark UI backgrounds.

Inspect every output yourself. If outlines still visibly differ as in the current
fan/lamp comparisons, iterate inside this ONE delivery; do not declare them
verified merely because contact sheets exist, hashes agree or regression passes.
Do not promise exact recovery of original hidden vectors/alpha: those were never
provided. Preserve source appearance rather than “improving” it.

## E — regression, scope boundaries and final delivery

Preserve current layout, font family, text>=18px, state colors, gradients and
functional repairs. Color review is sim_pc-only; no deployed LCD observations.
Do not rework C2–C6 unless an actual present regression is reproduced.

Test every role where replaced assets are used: Overview, four Trends selectors
selected/unselected, Devices, Settings, all three Auto editors, invalid draft,
offline, pending and Retry states. Verify no clipping/overlap/state-color regression.
Asset replacements must not change device semantics, thresholds or touch targets.

After the final relevant edit run:
- tools/build_sim.bat
- tools/run_regression.bat
- tools/export_mcu.bat --verify
- tools/verify_production.bat

Run canonical-source regeneration/check tests and final visible-state capture
inspection in the same delivery. Preserve all prior regression checks; report
permission limitations/untested cases honestly. One new docs/reviews/<delivery>
report with source/output/executable hashes, actual mask contact sheet, final
native screenshots, logs and ROM/heap deltas. No overwrite of past evidence.
Correct inaccurate closure/exact-source claims with supersession notes; update
README/indexes, UI_DESIGN_BRIEF, PLAN, DEV_LOG and relevant asset documentation.
CURRENT is the sole active prompt; mark IMPLEMENTED — awaiting review only when
done. No staged prompts or owner-mediated per-icon review cycle.

## Frozen MCU, wire and Auto contract

LVGL8.4,800x480 RGB565,42KiB heap, ONE800x10 draw buffer16,000B; no MCU framebuffer,
new runtime image allocations, float/trig in hot paths or ISR LVGL calls.
Process-lifetime static captions, fixed value buffers, bounded change-guarded
redraw; focused editors may release background widgets but not model/Auto/history.
Touch>=44px, steppers/footer>=48px; independent controls separated>=8px.

ui.c owns the six globals exactly once: lora_hub_cmd,lora_node_status,
lora_last_rx_tick_ms,lora_rx_revision,lora_last_snr,lora_last_rssi. Headers extern;
plain lvgl.h includes; existing8-byte command/20-byte status/ui_is_tx_ready gating.
Hub owns semantics/polarity/Auto; Node raw GPIO; switches show Node readback.
Uptime/Connected/signal bars/splash/English/°C/session history preserved.
No Details/notification panel/VOC Index/ppb/new health guarantees or RF drivers.

Auto: Fan CO2 ON>=1000/OFF<=800,bounds0..10000,step50,gapON>=OFF+50;
Purifier VOC ON>=150/OFF<=100,bounds1..500,step5,gapON>=OFF+5;
Humidifier RH ON<=40/OFF>=50,bounds0..100,step1,gapOFF>=ON+5.
Valid imported integers need not be step multiples. Preset changes load defaults;
unsupported/legacy/missing-invalid configManual; explicit validAuto retained.
Three fresh qualifying frames plus10s hold after enable/limit change/transition/
boot/reconnect/sensor recovery; no Auto decision offline/invalid/pending/latched.
No forcedOFF on loss; manual override has no timed return. Retry or deliberate
Manual→Auto re-arms; edits/reconnect/lateACK do not. No-opSave no callback/reset.
BackAuto keeps draft; BackSettings/Cancel discards; Save commits once;
Reset limits edits draft thresholds only. No Flash/CRC/persistence expansion.

Keep50-cycle free>4096B and abs(cycle10→50 drift)<=256B; distinguish host asset
size from target linked Flash/RAM. Hardware validation remains outside this task.
