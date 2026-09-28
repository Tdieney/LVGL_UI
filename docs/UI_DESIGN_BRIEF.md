# Smart Hub UI Design Brief

## 2026-09-28 — TVOC quality bands and TVOC naming (owner request)

The owner asked for sensible ppb bands, using the MXVC10S manual as a reference. The manual gives the TVOC range (0–60,000 ppb) and accuracy but no air-quality bands. Its accuracy breakpoints (2,008 / 11,110 ppb; 1,479 / 5,144 / 17,597 ppm) match Sensirion SGP30-class ethanol-equivalent sensors. The bands therefore follow the German UBA TVOC levels as published by Sensirion for that class: level 1–2 ≤ 220 ppb, level 3 ≤ 660 ppb, level 4 ≤ 2,200 ppb, level 5 above.

| Category | TVOC (ppb) | eCO₂ (ppm, unchanged) |
|---|---|---|
| Good | ≤ 220 | ≤ 800 |
| Moderate | 221–660 | 801–1200 |
| Poor | > 660 | > 1200 |

The Overview air-quality status is the worse of the two categories. The remaining `VOC` copy now reads `Based on eCO₂ and TVOC` and `Auto · TVOC`. The bands are constants in `ui.c` (`UI_TVOC_*_MAX_PPB`, `UI_ECO2_*_MAX_PPM`), and CHECK 32 asserts the 220/221/660/661 ppb edges. Not changed: Purifier Auto defaults (ON ≥ 150 / OFF ≤ 100 ppb), which now both lie inside "Good". Aligning them (for example ON ≥ 660 / OFF ≤ 220) is a separate owner decision.

## 2026-09-26 — Trends: the current minute follows the live value (owner decision)

Field report: after blowing on the sensor, eCO₂ rose sharply on the selector card, but the chart kept its old range. Cause: autonomous history stored only the first valid reading of each session minute, so the rightmost `Now` point and the Y range lagged up to 60 s. A spike shorter than a minute never appeared. All four charts were affected.

Decision (owner, from three options: latest / per-minute maximum / per-minute mean): each point shows the **latest valid reading** of its minute. The `Now` point therefore matches the selector card within about one report, and the Y range rescales immediately. A closed minute keeps its last reading, and a short peak that has already decayed before the minute ends is not retained. Unavailable readings do not erase a minute that already has data. Identical readings do not trigger a redraw. The 15-minute window, labels, round-number ticks and appearance are unchanged. Regression CHECK 42 covers all four metrics.

## 2026-09-25 — owner changes: LoRa v2 telemetry and simplified Trends

These owner-made code changes supersede the matching parts of the 2026-09-21
accepted appearance and the older decisions below (D-002, D-003, VOC Index
copy, Trends Current/Min/Max). They are implemented and host-verified, awaiting
technical-lead review: the simulator and regression suites pass 42/42 checks (2026-09-26).
See [PLAN.md](../PLAN.md) and [delivery report](reviews/2026-09-25-lora-v2-trends-delivery/REPORT.md).

### Firmware telemetry update

The current LoRa v2 firmware reports MXVC10S TVOC as ppb and equivalent CO2 as eCO2 ppm.
The earlier unitless VOC Index descriptions below are historical design references;
the current wire contract and UI behavior are specified in `../LORA_PROTOCOL.md`.
Purifier Auto thresholds are configured in ppb, and a stored v2 device configuration
is migrated to the v3 schema with the purifier thresholds reset to ppb defaults.

- Visible labels: `eCO₂` (ppm) and `TVOC` (ppb) on Overview, Trends and the Auto
  editor strip. The Overview TVOC value now shows a `ppb` unit. Like eCO₂, it
  switches from 34 px to 24 px above four digits and the unit follows the
  measured number width.
- Validity: eCO₂ outside 400..60000 ppm or TVOC above 60000 ppb is shown as
  unavailable (em dash), like a cleared `*_valid` bit.
- Quality bands: eCO₂ Good ≤ 800, Moderate ≤ 1200 ppm. TVOC was first carried over
  from the index scale (100/250). It was replaced on 2026-09-28 by UBA-based ppb bands
  (Good ≤ 220, Moderate ≤ 660); see the 2026-09-28 section above.
- Copy: the hero subtitle and Purifier mode line now read `Based on eCO₂ and TVOC` and
  `Auto · TVOC` (2026-09-28), matching the cards.
- Radio: the Hub polls with `GET_STATUS` about once per second and sends `HUB_CMD`
  only for pending commands. The Node replies only to requests. `sample_seq` marks
  new sensor samples, so repeated replies cannot speed up Auto.

### Canonical Auto values (supersedes the 2026-09-17 table below)

| Appliance / sensor | Default ON | Default OFF | Range | Step | Gap |
|---|---|---|---|---|---|
| Ventilation / eCO₂ ppm | at or above 1000 | at or below 800 | 0..60000 | 50 | ON - OFF >= 50 |
| Air purifier / TVOC ppb | at or above 150 | at or below 100 | 1..60000 | 5 | ON - OFF >= 5 |
| Humidifier / RH % | at or below 40 | at or above 50 | 0..100 | 1 | OFF - ON >= 5 |

### Trends refinements (2026-09-25, implemented)

After reviewing the simplified page, the owner decided five design directions (restored selector indicator, lower title/chart, raised Y unit, fixed 15-minute window ending at Now, round-number Y ticks); specific pixel coordinates and the tick step algorithm were technical-lead proposals:
1. **Restored selector indicator:** Selected card uses `ui_style_card_hero` (`LV_STATE_CHECKED`), rendering white icon, name, value, unit, and a celadon 8×8 px dot at (160, 14); unselected cards use porcelain styling with ink text and muted units. The 6×48 bar is removed.
2. **Title and chart lower:** Chart title lowered to (20, 14); `Data unavailable` badge aligned to top-right at (-20, 23).
3. **Y unit label raised:** Unit label positioned at (12, 96), maintaining at least 16 px clearance above top tick label.
4. **X axis fixed 15-minute window:** 16 chart points with newest sample at slot 15; autonomous labels `-15 min · -10 min · -5 min · Now`; external feeds use `HH:MM` timestamps without duplicate labels.
5. **Y axis round-number ticks:** Exactly 3 round labels `lo`, `lo+s`, `lo+2s` with $s \in \{1, 2, 2.5, 5\} \times 10^k$, matching grid lines.

### Trends page (native, 2026-09-25 active)

- Left rail: four porcelain selector cards (184×80, 88 px pitch) with icon, name
  and live value/unit. Active tab is highlighted with full hero styling (`UI_COLOR_FOREST`),
  white text/icons/vals/units and a celadon indicator dot at (160, 14).
- Selection fires on touch-down (`LV_EVENT_PRESSED`). Two-stage deferred rendering
  updates the selected tab immediately and renders the chart once touch inputs
  settle for 150 ms (`UI_TRENDS_SETTLE_MS`).
- Chart card (552×344): 34 px title at (20, 14); `Data unavailable` badge at
  top-right (-20, 23). Current/Min/Max, dividers, span label and tooltip remain removed.
- Axis block: unit label at (12, 96); 3 right-aligned Y labels at y 136/191/246 on
  the three grid lines; chart 460×132 at (72, 136) with 11 px padding and surface fill;
  4 time labels at y 276 centred at x 43/189/335 and right-aligned at 452.
- Chart drawing: native 2 px line and 4 px markers on normalized 0..1000 range.
  Empty state box (512×191) at (20, 108): `No readings yet` (0 samples) or
  `Collecting readings…` with helper hint (1 sample).

## 2026-09-21 — independent acceptance of copy, remaining integration defects

The simplified unavailable/error copy below is independently accepted; no further
visual redesign is requested. Old uptime widening is fixed. A separate stale
link-label measurement now clips Disconnected at the screen edge; preserve the
accepted22px right margin while fixing it. Constant-style config/RAM corrections
must not change colors, fonts, icons or normal content. See
[const-style review](reviews/2026-09-21-const-style-review/REVIEW.md) and CURRENT.

## 2026-09-21 — implemented: unavailable-state copy simplification & width-safe header

- **Dashboard Temperature/Humidity:** Unavailable values show em dash `"—"`; unit hidden; no "Offline" or "No data" notes.
- **Dashboard device rows and Devices cards:** Removed "Unknown"; ONE dash indicates unavailable readback; mode/Auto paused preserved.
- **Command error:** Shows ONE dash `"—"` at state label position + "Retry" button (dash widget hidden, 0 pixel overlap).
- **Trends:** Retained `"Data unavailable"` badge and `"No readings yet"` placeholder.
- **Width-safe Uptime Header:** Implemented length-bucketed reservation model in `get_uptime_reserved_width()`. Guarantees $\ge 14\text{px}$ minimum clear separator gap across all proportional font shifts. Clock distance stationary at 8px.
- Verified in [delivery review](reviews/2026-09-21-runtime-followup-delivery/REVIEW.md) and 34/34 regression tests.

## 2026-09-21 — OWNER ACCEPTS current native appearance

The owner has completed visual refinement with the developer and says the current
native appearance meets their requirements. This supersedes earlier demands to
restore generated Quiet Architecture concept geometry/icons/colors. Preserve the
accepted native design, typography>=18px, sizes, spacing and icon masters.

The new task is runtime correctness, smoothness and RAM reduction, NOT redesign.
The owner reports MCU RAM99.9%; map/config/runtime stack evidence is not yet
available. Technical-lead candidates (constant styles, bounded allocation and
smaller measured profiles) are implementation advice, not validated target budgets.
Normal-state visuals must remain equivalent; corrections to demonstrably broken
data/boundary states are permitted. No persistence, wire or Auto feature expansion.
See [runtime/RAM review](reviews/2026-09-21-runtime-ram-review/REVIEW.md) and
[sole active handoff](prompts/CURRENT.md).

## 2026-09-20 — Quiet Architecture Icon Provenance Recovery & Unified Pipeline Delivery

Complete consolidated delivery implementing the provenance correction and icon appearance recovery per `docs/prompts/CURRENT.md`:
Visual evidence, decoded A4 contact sheet, aspect-normalized contour overlays, and full test logs are in:
- [Delivery Report (REPORT.md)](reviews/2026-09-20-icon-recovery-delivery/REPORT.md)
- [Visual Comparison, Decoded A4 Aspect Normalization & Interactive Overlay (COMPARE.html)](reviews/2026-09-20-icon-recovery-delivery/COMPARE.html)
- [Icon Family Contact Sheet (icon_contact_sheet.png)](reviews/2026-09-20-icon-recovery-delivery/icon_contact_sheet.png)
- [Full Regression Log (33/33 PASS)](reviews/2026-09-20-icon-recovery-delivery/REGRESSION.txt)
- [Source Identity Provenance Hashes (SOURCE_IDENTITY.md)](reviews/2026-09-20-icon-recovery-delivery/SOURCE_IDENTITY.md)

Key implemented architectural alignments:
1. **True Provenance Grounding (No Invented Vectors):**
   - Recognized that the approved Quiet Architecture concepts (`docs/drafts/2026-09-17-pro-max/*.png`) were generated by raster diffusion AI (`image_gen`) without an underlying vector package (no Phosphor, Lucide, Heroicons, or bespoke SVG).
   - Built a linear matting extraction algorithm ($C = \alpha \cdot F + (1 - \alpha) \cdot B$) operating on source pixel ROIs to produce 18 primary 8-bit alpha masters without manual Bézier approximations or generative guessing.
   - Pinned 7 documented extension masters (`chart`, `devices`, `radio`, `plug`, `heat`, `close`, `check`) rendered at 128×128.
2. **Specific Appearance Errors Eradicated:**
   - **Home**: Restored solid dark-filled rectangular doorway with flat lintel, eliminating the incorrect arched hollow doorway (`a2.5 2.5`).
   - **Fan**: Restored smooth rounded paddle blade tips, spacious internal blade loops, and slim roots meeting hollow hub ring (0.838 aspect ratio).
   - **Desk Lamp**: Restored rounded crown of shade, tapered asymmetric hood, upright stem with bent shoulder, and hollow pedestal base with flat bottom.
   - **Purifier**: Restored fine elliptical top rim, small top oval, 5 perforations following subtle perspective curvature, and fine lower curved intake slit (normalized sidewall weight reduced from 7.3% to canonical 4.8%).
   - **Humidifier / Droplet**: Restored svelte, gradual transition from pointed apex to circular bulb.
   - **Air Streams & Check Badge**: Matting preserves flowing wind curves and celadon badge with transparent cutout checkmark.
   - **Warning**: Correct equilateral red triangle with centered exclamation mark.
3. **Unified Single Pipeline & Authoritative Manifest:**
   - Retired redundant `SVG_PATHS` dictionary and Microsoft Edge headless dependency in `generate_assets.py`.
   - Created authoritative `manifest.json` (`schema_version: "2.0"`) linking source PNG hashes, ROIs, extraction parameters, alpha master hashes, ink bounding boxes, runtime roles/sizes, and emitted A4 data hashes.
   - `generate_assets.py` loads masters and manifest directly, applies Lanczos downsampling to LVGL 8.4 A4 format (`LV_IMG_CF_ALPHA_4BIT`), and emits `ui_icons.c` and `ui_icons.h`.
   - Implemented non-mutating `--check` mode verified against disposable scoped tampering fixtures.
4. **Direct A4 Byte Decoding for Verification:**
   - `build_review_pack.py` now decodes emitted `ui_icons.c` A4 byte arrays directly for `icon_contact_sheet.png` and diagnostic aspect panels (`purifier_aspect_compare.png`, `fan_aspect_compare.png`, `light_aspect_compare.png`, `drop_aspect_compare.png`, `home_aspect_compare.png`). Zero SVG preview substitution.
5. **Full System Verification & Zero Regressions:**
   - Simulator build (`tools/build_sim.bat`): PASS.
   - Regression suite (`tools/run_regression.bat`): 33 / 33 checks PASS.
   - MCU export (`tools/export_mcu.bat --verify`): PASS.
   - Production verification (`tools/verify_production.bat`): All 4 phases PASS.
   - Memory contract preserved: 42KiB heap, single partial draw buffer (800×10, 16,000B), peak heap used 88%, minimum free heap 5,280 bytes (>4,096B limit), 50-cycle drift -80 bytes (<=256B limit).

## 2026-09-20 — original icon provenance and source-based restoration

## 2026-09-20 — owner clarification: appearance and simulator-only assessment

The owner explicitly means the icons' visual form, not their size: silhouettes,
curves, construction, internal details and stroke character must resemble Quiet
Architecture. Enlarging icons is not acceptance. Compare original and compiled
icons at a common height with aspect ratio preserved to isolate shape differences.
The current color complaint concerns sim_pc on PC only; no physical LCD deployment
has been made for this assessment. Investigate source palette, shading/rendering
and simulator RGB565 quantization; do not invoke LCD hardware as the explanation.
This clarification changes no Auto/wire/MCU contract and authorizes no UI code
changes during the review/prompt-writing turn.

## 2026-09-20 — independent review and canonical asset guidance

The owner asks to review current icon/color fidelity and whether RGB565 prevents
matching Quiet Architecture. This is not approval of a new design or palette.
The seven original raster concepts remain visual references; they are not a
consistent vector package (purifier details differ between Devices and Auto VOC).
Technical-lead guidance in [CURRENT](prompts/CURRENT.md): choose Devices as the
canonical appliance silhouette source, reuse it at measured optical role sizes,
freeze source SVGs and validate compiled masks. Use near-black appliance ink.
Preserve near-current canvas/celadon; measure/tune forest hue and bounded tonal
depth on actual RGB565 output rather than globally increasing saturation.
All visible text remains>=18px. No new product/Auto/wire behavior is authorized.

[Sep20 review and quantitative comparison](reviews/2026-09-20-icon-color-review/REVIEW.md)
supersedes prior blanket Q1–Q5/pixel-perfect closure claims below. Ordinary boundary
and editor repairs pass; Trends unit contrast, gap-domain/isolated-point behavior,
icon fidelity and remaining proof/reporting corrections are still open.
Study12/no-gradient history below is not the current visual authority. Current
family is Plus Jakarta Sans and46icons, not the older Segoe/40icon figures.

## 2026-09-23 — Trends Autonomous Session History & Auto Live Path Delivery

Single consolidated delivery implementing P1 Trends autonomous bounded history and P1 Auto confirmed transition hold separation:
- [Trends & Auto Delivery Report (REVIEW.md)](reviews/2026-09-23-trends-auto-delivery/REVIEW.md)
- [Investigation Notes (REVIEW.md)](reviews/2026-09-23-trends-auto-investigation/REVIEW.md)
- [Full Regression Log (35/35 PASS, 100%)](reviews/2026-09-23-trends-auto-delivery/REGRESSION.txt)

Key architectural decisions implemented:
1. **Autonomous Session History in UI:** `ui.c` autonomously samples valid received telemetry into the 16-point buffer (1 slot per elapsed session minute). On boot/first report, a true singleton is captured (`count = 1`), with Min/Max matching live value and span displaying `"Single reading"`. Missing minutes or RF gaps are preserved as gaps (`valid = false`). External feed (`ui_feed_history`) takes exclusive precedence when called by firmware.
2. **Elapsed Session Time Axis:** Autonomous Trends chart renders honest elapsed session time (`+0m`, `+15m`, `+1h05m`) using `ui_font_18` (all text $\ge 18\text{px}$), avoiding ungrounded wall-clock RTC claims while preserving wall-clock copy (`%02u:%02u`) for explicit external feed.
3. **Auto Mode Switching vs Confirmed Transition Hold:** Setting Manual $\rightarrow$ Auto in settings arms immediately without an artificial 10-second hold. The 10s anti-hunting hold applies strictly to confirmed physical relay transitions, link reconnect, and sensor recovery. Evaluates on 3 consecutive qualifying frames. Active High and Active Low polarities verified.

## 2026-09-20 — Quiet Architecture C1–C6 Consolidated Delivery

Single consolidated delivery implementing C1–C6 per `docs/prompts/CURRENT.md` with owner clarification incorporated:
Visual evidence, interactive side-by-side comparison, icon contact sheet, and full logs are in:
- [Executive Delivery Report (REPORT.md)](reviews/2026-09-20-quiet-architecture-consolidated/REPORT.md)
- [Visual Comparison, Icon Aspect Normalization & Interactive Overlay (COMPARE.html)](reviews/2026-09-20-quiet-architecture-consolidated/COMPARE.html)
- [Icon Family Contact Sheet (icon_contact_sheet.png)](reviews/2026-09-20-quiet-architecture-consolidated/icon_contact_sheet.png)
- [Full Regression Log (33/33 PASS)](reviews/2026-09-20-quiet-architecture-consolidated/REGRESSION.txt)
- [Source Identity Provenance Hashes](reviews/2026-09-20-quiet-architecture-consolidated/SOURCE_IDENTITY.md)

Key implemented architectural alignments:
1. **C1 Asset Pipeline & Canonical Vector Icons (Owner Clarification Implemented):**
   - Reconstructed canonical silhouettes matching reference concepts: 1-row 5-perforation purifier tower, organic 4-paddle aerodynamic fan on circular hub ring (0.84 aspect ratio), anglepoise desk lamp with base pedestal and conical shade, pure svelte liquid teardrop, and symmetrical house with arched door.
   - 25 canonical vector SVGs authored and pinned in `assets/icons/quiet-architecture/` with `manifest.json`.
   - Re-rendered all 46 icons as anti-aliased 4-bit alpha masks (`LV_IMG_CF_ALPHA_4BIT`) using headless Microsoft Edge with 4x supersampling and high-quality Lanczos downsampling.
   - Generated aspect-normalized diagnostic panels at standard 160px height (aspect ratio preserved) with contour difference overlays in Section 0 of `COMPARE.html`.
2. **C2 Calibrated Palette, Zero Splash Seam & Hero Gradient:**
   - Calibrated palette tokens in `ui_theme.h`: Canvas `#EEECE7` (`0xEF7C`), Surface `#F3F1EC` (`0xF79D`), Forest `#214539` (`0x2227`), Forest Dark `#183831` (`0x19C6`), Celadon `#D7E0BC` (`0xD717`), Ink `#17251F` (`0x1124`).
   - Synchronized startup splash screen background and embedded asset (`ui_splash_logo.c`) to bit-exact `#EEECE7` (`0xEF7C`), completely eliminating the 1-bit boundary seam.
   - Overview Hero card upgraded to 2-stop vertical gradient (`#214539` top to `#183831` bottom) supported by a static 1024-byte gradient cache (`lv_gradient_set_cache_size(1024)` in `ui_theme_init()`).
3. **C3 Trends Selector Tab Units & Contrast:**
   - `update_trends_tabs()` in `ui.c` updates unit caption colors dynamically: pure white (`#FFFFFF`) on selected active tab, muted ink (`UI_COLOR_MUTED`) on deselected tabs, guaranteeing $\ge 4.5:1$ contrast ratio across all states.
4. **C4 Trends Isolated Points & Time Axis Domain:**
   - Area shading polygon boundary adjusted (`poly[1].x = p2.x - 1`), eliminating vertical double-blended seam lines between adjacent quads.
   - Last valid point highlighted; isolated points and singletons rendered explicitly via `lv_draw_rect()` in `on_chart_draw_part_begin()`.
   - Time axis strictly represents 0..15 slot domain with endpoints anchored directly to slot timestamps.
5. **C5 Typography Hierarchy & Decoupled Units:**
   - Navigation pills upgraded to `ui_font_24` (widths 118, 96, 108; x-offsets 0, 130, 238; height 36px, radius 18px).
   - Hero metrics: `s_home_co2_val` set to nominal `ui_font_34` with `ui_font_24` fallback for $>4$ digits; `s_home_voc_val` set to `ui_font_34`.
   - Auto editor threshold values & live reading decoupled: separate value label (`&ui_font_34`, `UI_COLOR_INK`) and unit label (`&ui_font_18`, `UI_COLOR_MUTED`) with dynamic baseline alignment and horizontal centering.
   - All appliance, settings, dialog, and strip icons recolored to `UI_COLOR_INK`.
6. **C6 Test Assertions & Watchdog Hardening:**
   - Added hooks in `sim_pc/ui_test_hooks.h` and `ui.c` (`trends_tab_units`, `trends_time_labels`, `home_co2_val`).
   - CHECK 32 updated for decoupled live reading labels (`live_val32` and `unit32`).
   - CHECK 33 extended with probes 14 (C3 role colors), 15 (C4 isolated point & domain bounds), and 16 (C5 font scaling).
   - `tools/verify_production.ps1` hardened: process identity verified via `StartTime` before killing PID; bounded stream draining.

## 2026-09-18 — Final Quiet Architecture Reconstruction & Verification Delivery

Single consolidated delivery closing all remaining defects (Q1–Q5) from the independent reconstruction review:
Visual evidence, interactive comparison and native 800×480 RGB565 captures are available in:
- [Visual Comparison & Interactive Overlay (COMPARE.html)](reviews/2026-09-18-final-reconstruction/COMPARE.html)
- [Icon Family Contact Sheet (icon_contact_sheet.png)](reviews/2026-09-18-final-reconstruction/icon_contact_sheet.png)
- [Delivery Report (REPORT.md)](reviews/2026-09-18-final-reconstruction/REPORT.md)
- [Full Regression Log (33/33 PASS)](reviews/2026-09-18-final-reconstruction/REGRESSION.txt)
- [Source Identity Provenance Hashes](reviews/2026-09-18-final-reconstruction/SOURCE_IDENTITY.md)

Key implemented achievements:
1. **Boundary & Recovery Collisions Eliminated (Q1):** Recomputed strictly disjoint Y-tier layout for Trends card (Title y=2..47, Stats y=50..121, Unit y=130..154, Chart y=160..292, Time y=304..328) with zero overlap even at 10,000 ppm. Home & Devices latched error states provide dedicated non-overlapping horizontal/vertical lanes for switches, retry buttons, and device modes. Settings Save rejected feedback renders in dedicated error slot without encroaching on Polarity row. Environmental tiles separate '—' reading and 'Offline' notes without glyph intersection.
2. **Truthful Trends Timing & Coverage (Q2):** Truthful sample elapsed time calculation with 24-hour midnight wrap handling; exact single-reading handling ('Single reading' instead of synthetic multi-minute duration); bounded chart point plotting and time tick placement.
3. **Visual Hierarchy & Assets (Q3):** Studio 34px headers, 24px nav tabs, Hero active check badge (`ui_icon_check_circle_32`) and subtitle "Based on CO₂ and VOC", wire vector icons (`ui_icon_chevron_left_24`, `ui_icon_plus_24`, `ui_icon_minus_24`, `ui_icon_chevron_down_18`, `ui_icon_leaf_18`), chart endpoint emphasis and celadon area shading (`#E8EFE9`).
4. **Verification Hardening (Q4):** Multi-ancestor containment and pairwise 2D sibling collision checks in `audit_visible_glyph_containment`; 5 negative controls in CHECK 32; hardened CHECK 31 across all 5 scheduled lifecycles with bidirectional transitions and no-op rejection; CHECK 33 covering all 13 critical probe scenarios; watchdog process-tree tracking in `tools/verify_production.ps1`; and regression failure propagation in `tools/build_review_pack.py`. All 33/33 checks pass.
5. **Documentation & Provenance (Q5):** Updated `MCU_BASELINE.md` clearly identifying host x86_64 object sizes vs target ARM Cortex-M Flash/RAM; truthful offline capture descriptions; aligned review deliverables under `docs/reviews/2026-09-18-final-reconstruction/`; updated `PLAN.md`, `DEV_LOG.md`, and marked `CURRENT.md` as implemented.

## 2026-09-18 — independent reconstruction review: not yet accepted (Historical)

The seven original Quiet Architecture concepts remain the accepted visual direction.
Current native composition and several concrete repairs are accepted improvements,
but do not imply complete visual/functional acceptance. The author's closure claims
below are superseded where contradicted by the
[independent review](reviews/2026-09-18-reconstruction-review/REVIEW.md).

Remaining work: native boundary/recovery overlap, truthful history coverage,
already-requested typography/unit/icon hierarchy and bounded tonal depth, plus
proof/reporting corrections. All collected in [one CURRENT](prompts/CURRENT.md).
The18px floor, Auto/wire contract and major geometry remain unchanged.
No new product decision was made; technical-lead fit/copy guidance is identified
as such, not misattributed as a new owner approval.

## 2026-09-18 — Quiet Architecture reconstruction & verification closure implemented

Consolidated delivery completed per `docs/prompts/CURRENT.md` (R1–R7 + V1).
Visual evidence, interactive comparison and native 800×480 RGB565 captures are available in:
- [Visual Comparison (COMPARE.html)](reviews/2026-09-18-quiet-reconstruction/COMPARE.html)
- [Icon Family Contact Sheet (icon_contact_sheet.png)](reviews/2026-09-18-quiet-reconstruction/icon_contact_sheet.png)
- [Comprehensive Delivery Report (REPORT.md)](reviews/2026-09-18-quiet-reconstruction/REPORT.md)

Key implemented achievements:
1. **Canonical 2-Row Shell & Geometry (R2):** Reconstructed the 2-row shell with Header (`y: 0..56`) and separate Tabs row (`y: 56..100`, `h: 44`) with active celadon capsule `#DCE7DD`. Main content container at `(24, 112, 752, 344)` with 24px bottom margin. Overview asymmetric split: Left 416px (Hero `416×216`, two shallow tiles `200×112`) + 16px gap + Right Devices panel 320px (4 rows `h: 70`). Devices page restored to 2×2 grid of `368×164` cards. Trends page restored to 184px left rail + 552px chart card.
2. **Clipping & Overlap Elimination (R1):** Auto live sensor strip expanded to 80px (`pos: 0, 52; size: 752, 80`) providing 17px vertical clearance for font 34; dynamic unit placement based on measured digit width for CO₂ and VOC; widened device mode text lane to 176px (eliminating "Auto paused" wrapping/clipping); relocated toast notifications to empty right half of nav row (`y: 58`) without occluding tabs or shortcuts; restored all interactive touch targets to `≥ 44px`.
3. **Vector Icon Redraw (R3):** Redrew all 40 vector icons from scratch in `tools/generate_assets.py` matching the approved silhouettes: cylindrical purifier with elliptical rim and body perforations; 4-blade balanced fan; clean teardrop without inner swirl; connected upright desk lamp; 3-track sliders with circular knobs; sharp chevrons; and bold centered +/- steppers. Compiled 4-bit alpha masks take only 17.50 KiB Flash.
4. **Trends Corrections (R4):** Short sensor titles ("CO₂", "VOC", "Temperature", "Humidity"); distinct 3-column Current/Min/Max stats with vertical separators; removed hardcoded "Last 15 minutes" label (truthful coverage displayed only when history exists); removed VOC "index" copy.
5. **Settings & Auto Scale (R5):** 1px solid #D8D3C8 outlined preset dropdown; 24px row icons; explicit ON/OFF limits summary ("ON: %d  OFF: %d %s"); unboxed back chevron.
6. **Unsupported Copy Removal (R6):** Eliminated "within healthy range" and VOC "index" throughout the UI.
7. **Verification Hardening (R7 / V1):** Strict ZIP membership validation (reject unexpected/duplicate entries); watchdog process tree termination with child PID death assertions; CHECK 1 near-UINT32_MAX clock rollover with link staleness masking; CHECK 28 PENDING phase assertion; CHECK 31 five scheduled lifecycles counting; CHECK 32 visible glyph containment verification in all clipping ancestors.

## 2026-09-18 — R2 owner rejection: reproduce original images and regenerate icons (Historical Review)

The owner rejects the fidelity overhaul below: it is not visual acceptance.
The seven original Quiet Architecture images remain authoritative. The latest
request explicitly includes regenerating the icon family to match their shapes,
not merely enlarging old assets. Preserve the18px text floor and all runtime
contracts. Restore the two-row header/navigation, original major proportions,
icon silhouettes and typography; fix clipped states/readings together.

See [R2 review and comparison](reviews/2026-09-18-fidelity-r2-review/REVIEW.md)
and [one active handoff](prompts/CURRENT.md). Numeric anchors in CURRENT are
technical-lead translation targets, not a newly owner-approved visual direction.
The following author's MATCH/completion/verification-closure claims are historical
submission claims and are superseded where contradicted by R2 evidence.

## 2026-09-18 — Quiet Architecture fidelity overhaul & verification closure implemented

Consolidated implementation delivery completed per [docs/prompts/CURRENT.md](prompts/CURRENT.md) (F1–F6 + V1).
Visual evidence and side-by-side comparison with the 7 approved Quiet Architecture concepts
are available in [COMPARE.html](reviews/2026-09-18-quiet-fidelity-fix/COMPARE.html) and
[REPORT.md](reviews/2026-09-18-quiet-fidelity-fix/REPORT.md).

Key implemented changes:
1. **Typography & Roles**: Strict $\ge 18\text{ px}$ floor across all visible text. Added `ui_font_48`
   (Segoe UI Semibold) for verdict and hero readings. Font roles: 18px (secondary/units/errors),
   24px (labels/actions), 34px (headings/thresholds), 48px (hero verdict/readings).
2. **Icon Set**: 38 embedded icons including 44px outline appliances (`purifier_44`, `fan_44`, `drop_44`,
   `light_44`, `plug_44`, `heat_44`), 32px outline sensors (`thermometer_32`, `drop_32`, `air_32`, `leaf_32`),
   and action symbols (`chevron_left_24`, `chevron_right_20`, `chevron_down_18`, `gear_24`, `leaf_24`).
3. **Overview & Layout**: Forest hero (368×236), lower porcelain tiles (176×152) for Temp/Humidity,
   right device panel (368×404) with outline icons, aligned mode/state lanes, and centered switches.
4. **Trends**: 4 selectable left cards (240×92) with live readings and active forest card indicator;
   right porcelain chart card (496×400) with Current/Min/Max in 34px and single clean data series line.
5. **Devices**: 2×2 grid of 368×192 cards with outline icons, mode with sensor association, and full-width
   Settings row with sliders icon and chevron.
6. **Settings**: Contextual header with unboxed back chevron; 4 form rows with fine dividers; visible
   dropdown outline; segmented Manual/Auto; threshold summary; segmented Active high/low polarity.
7. **Auto Editor & Validation**: Unboxed back chevron; celadon strip with outline icon and live reading;
   two equal 368×148 limit cards with outlined value boxes and 48px steppers; zero-reflow validation
   (card borders remain constant, error outline on value boxes only); stable helper/error slot;
   symmetrical 3-button footer.
8. **V1 Closures**: Streamed SHA256 verification in `verify_production.ps1`, same-length tampering detection,
   process-tree containment, CHECK 1 rollover/staleness 4999/5000ms, CHECK 28 pending-then-latch, and CHECK 31
   50-cycle stress with >4096B free and zero drift.

## 2026-09-18 — visual acceptance correction and consolidated handoff (Historical)

The owner rejected the submitted implementation's fidelity and requested review
plus a prompt for another coding AI. The seven Quiet Architecture concepts in
`docs/drafts/2026-09-17-pro-max/` remain the accepted visual direction: Dashboard,
Trends, Devices, Settings, CO2 Auto, humidity Auto and invalid VOC Auto.
Native captures are implementation evidence, NOT an approved replacement design.
See the [review/comparison](reviews/2026-09-18-design-fidelity/REVIEW.md) and
[one complete fix handoff](prompts/CURRENT.md).

The preceding owner authorization to simplify copy and enforce **18px minimum**
still applies. It does not authorize removing selector readings, contextual
headings, form structure or visual hierarchy. The technical-lead corrections
restore these components, larger role-based typography/icons, aligned columns,
explicit polarity choices and stable inline validation. Suggested native font
sizes/geometry in CURRENT are implementation targets, not owner-approved pixels.
Short filler may stay removed; operational feedback, units, inclusive ON/OFF
rules and unavailable/error states must remain truthful.

This section supersedes the following implementation section's claims that
native output replaces raster geometry, that Trends selectors should lose their
readings, and that generic editor titles/polarity switches/flat-only surfaces
are the accepted visual specification. Necessary low-cost rendering adaptations
must be recorded, not treated as automatic visual acceptance. Actual memory,
wire and Auto behavior constraints remain unchanged. No production code was
changed in this review; no new product or automation policy was introduced.

## 2026-09-17 — Quiet Architecture implemented; system/copy review

Historical author decisions below: visual completion and conflicting visual
prescriptions are superseded by the 2026-09-18 correction above.

Owner authorized implementation after accepting the complete design package.
This section supersedes the draft-only status and raster geometry below. The
native LVGL implementation and its [800x480 captures](reviews/2026-09-17-quiet-implementation/REPORT.md)
are the current visual reference; the older browser prototype is historical.

### Information and typography decisions

- Minimum visible text is **18 px**, including chart axes, hints, errors, menu
  items and transient feedback. Use 24 px for navigation, headings and form
  labels; 34 px for primary readings/thresholds. Never shrink text to fit.
- Remove decorative reassurance, repeated page headings and instructions that
  merely repeat the visible controls. Keep per-metric quality words: they explain
  which reading caused an aggregate warning. No temperature/humidity comfort
  verdict is inferred from those readings.
- Overview uses one forest air-quality panel, two environmental tiles and a
  four-device list. Aggregate quality is the worse existing CO2/VOC category,
  but reads `Unavailable` if disconnected or either required reading is invalid.
  This is a demo presentation rule, not a new calibrated air-quality algorithm.
- Keep `CO₂` / ppm and unitless `VOC`; do not invent a Sensirion calibration or
  derive an index from raw ppb in the UI. Firmware still provides validated data.
- The header keeps Studio, session uptime and Connected/Disconnected. Top-level
  navigation is labelled Overview / Trends / Devices; there is no bottom bar.
- Trends removes the duplicate page title and selector readings. The selected
  metric, Current/Min/Max and actual history timestamps are sufficient. Do not
  promise a fixed 15-minute window or invent data when history is empty.
- Devices keeps mode and fault feedback, not redundant ON/OFF text. Switches
  display Node readback; Sending, Unknown, Auto paused and Retry remain distinct.
- Settings uses `Mode`, `Auto limits`, `Save`, `Cancel` and `Active-low output`.
  Keep polarity accessible for integration; it is not a normal output toggle.
  Auto limits summary explicitly labels ON and OFF. Unsupported Auto is disabled.
- Auto uses two equal cards with `Turn ON` / `Turn OFF`, inclusive `At or above`
  / `At or below`, units beside values, and a live `Now` reading. `No live data`
  replaces stale values. The hint explains the hold-between-limits rule and gap.
- Invalid limits have visible error text/outlines, disabled Save, and a `Fix
  invalid limits` indicator after Back. End-of-range steppers are disabled.
- Short transient feedback occupies the unused right side of the navigation
  row, not the readings or device controls; no persistent notification banner.
  Editors use inline validation rather than overlay feedback.

### Engineering and interaction boundaries

- Full-width Settings/Auto replaces the 440x368 dialog. Back from Auto retains
  the shared draft; Back from Settings and Cancel discard; Save commits once.
  Reset limits changes only draft thresholds. No new persistence driver.
- Page widgets are released while the focused editor is open and rebuilt on
  return. Model, command tracking, history and Auto engine remain alive.
- Preserve wire schema, global ownership, hysteresis, 3 fresh frames, 10-second
  hold, manual override, polarity, timeout latch and explicit re-arm behavior.
- Native controls have at least 44px touch height (48px editor steppers/actions),
  with separated switch/Retry lanes. Open dropdown rows retain 44px pitch.
- Use flat warm surfaces and forest/celadon tokens with light shadows. Raster
  gradients are not copied into a framebuffer or new heavy rendering pipeline.
- Regression now audits visible fonts and editor transitions. Physical LCD
  contrast, viewing distance, QSPI timings and target touch still require hardware
  confirmation; a simulator pass does not prove those properties.

## 2026-09-17 — Settings and Auto editor design package

Owner requested completion of Settings/Auto design after the companion pages.
The following is a complete proposed interaction specification with four raster
drafts, NOT approval to implement a production redesign. Engine rules below are
verified against `ui_auto.h`, `ui_auto.c` and current settings handlers in `ui.c`.

### Screens and visual structure

- [Settings](drafts/2026-09-17-pro-max/settings-quiet-architecture.png): four
  aligned rows for device type, Manual/Auto, Auto thresholds and output polarity.
- [CO2 thresholds](drafts/2026-09-17-pro-max/auto-co2-quiet-architecture.png):
  two equally sized ON/OFF cards with inclusive comparison wording and steppers.
- [Humidity thresholds](drafts/2026-09-17-pro-max/auto-humidity-quiet-architecture.png):
  same structure with reversed comparison direction; units are %RH.
- [Invalid VOC draft](drafts/2026-09-17-pro-max/auto-voc-invalid-quiet-architecture.png):
  ON=OFF=150, red field outlines plus actionable text and disabled Save.
  This is intentionally invalid; normal VOC defaults are 150/100.

Focused full-screen editing replaces the old small modal as a DESIGN proposal.
Studio/uptime/link remain visible; the tab row becomes a back button and device
editing title. No top-level navigation, output toggle, or decorative chart inside
the form. Porcelain surfaces, forest actions and warm neutral canvas follow the
accepted direction. Target 48px touch controls, 18px minimum labels, consistent
24px outer/16px internal gaps. These are native-prototype targets, not measured
properties of the AI-generated bitmap.

### Draft lifecycle and navigation

1. Devices > Settings copies the current device configuration to one draft.
2. Device type, mode, polarity and threshold changes affect ONLY that draft.
3. Auto thresholds opens the nested editor using the SAME draft; its Back returns
   to Settings retaining all edits, including invalid values and error state.
4. Cancel from either editor discards the entire draft and returns to Devices.
   Settings-level Back has the same discard/exit behavior. This preserves current
   close-to-discard semantics; there is no implicit save or extra save layer.
5. Reset limits resets only the draft ON/OFF values for the selected type. It
   does not change mode, type, polarity or send a command. No global reset.
6. Save settings on EITHER view validates and commits the entire draft once,
   then returns to Devices. Unchanged Save closes without callback/Auto restart.
   Saved feedback is short and non-modal. A commit rejection retains the draft
   and displays its actual reason; never show success before a successful commit.
7. Supported types may edit limits while Manual without enabling Auto. Selecting
   Auto is itself a draft edit; only a valid Save enables it. In Settings, show
   an invalid-threshold indicator on the Auto thresholds row and disable Save
   if returning from an invalid editor, even while mode is Manual.

### Settings details

Device type uses the existing eight presets: Air Purifier, Ventilation Fan,
Humidifier, Dehumidifier, Heater, Desk Light, Smart Socket and Generic Device.
The dropdown has readable >=48px rows, scrolls within its bounded list, marks the
selected item and closes without changing draft on outside tap/Back. Names/icons
follow existing preset behavior; no new free-text keyboard/rename feature.
Preset changes use existing canonical defaults/Manual-only restrictions, never
reinterpret a VOC number as ppm or RH. Do not overwrite values just by reopening.

For supported presets, Manual and Auto are a single-select mode control. For
unsupported presets, Manual is selected; Auto is visibly disabled, with the
threshold row replaced by `Manual only for this device type` (no dead chevron).
The threshold summary in Settings means ON / OFF in that order; this ordering
stays fixed for humidity even though its numerical order is reversed.

Output polarity is a wiring setting, not a relay toggle. `Active high` maps to
active_low=0 and `Active low` to active_low=1. It stays draft-only until Save.
Do not add physical NC/NO wiring claims or change the existing commit mapping.

### Canonical Auto values (inclusive comparisons)

| Device / sensor | Turn ON | Turn OFF | Allowed range | Tap step | Valid gap |
|---|---|---|---|---|---|
| Ventilation / CO2 ppm | at or above 1000 | at or below 800 | 0..10000 | 50 | ON - OFF >= 50 |
| Air purifier / VOC | at or above 150 | at or below 100 | 1..500 | 5 | ON - OFF >= 5 |
| Humidifier / %RH | at or below 40 | at or above 50 | 0..100 | 1 | OFF - ON >= 5 |

Each tap changes only its own field and clamps to its range. Partner field is
never silently moved; no auto-swapping. Disable the direction button at its
range boundary. Existing imported valid integers need not be step multiples.
No slider, drag gesture, long-press acceleration or keypad is needed in v1.
VOC is unitless, never ppb/ppm. Defaults are demo rules, not health guarantees.

Normal helper text explains the minimum gap and retention of current output
between limits. Comparison copy includes equality (`at or above/below`). Reserve
two lines for helper/error text so cards/footer never jump during validation.
An invalid pair keeps steppers, Back, Cancel and Reset available; both value
boxes get error treatment, Save is disabled, and the message provides a repair:
`Raise ON or lower OFF to continue.` For humidity reverse this repair direction.
Do not silently repair the error when leaving/re-entering the threshold view.

### Runtime/fault states and limits of these drafts

- Current reading is observational, not the editable threshold. Show an em dash
  and `Sensor unavailable` or `Disconnected` when invalid/stale, never a false
  zero. Preserve the draft across live updates and reconnects; stable slots avoid
  layout shifts. Link state in the global header remains accurate while editing.
- Configuration is Hub-local. Valid settings can still be edited/saved without
  live sensor data; that does not authorize output changes while offline/invalid.
  Keep existing command guards, deadband, 3-frame qualification and 10-second
  hold/recovery policy. Save does not imply immediate relay actuation.
- Preserve pending/error/retry rules and no forced-OFF on lost connection.
  No-op Save or ordinary threshold edits must not bypass an Auto error latch.
  Manual output-switch override on other pages remains an explicit move to
  Manual; there is no timed return to Auto.
- No Flash logging/storage driver, packet schema, Node automation or new rule
  engine is proposed. Save means the existing Hub config commit/callback.

ui-ux-pro-max informed grouped controls, visible labels, stable feedback regions,
and actionable inline validation (verified Forms/Error Recovery search match).
Images were produced with built-in imagegen and visually checked for text,
comparison direction, displayed defaults and error/disabled state. Raster image
inspection does NOT validate touch, font fit at extremes (10000 ppm), contrast
on hardware, memory use, live state transitions or actual save behavior. Those
remain native-prototype/implementation checks. Settings/Auto screen direction
awaits owner feedback; current LVGL, browser prototype and CURRENT are unchanged.

## 2026-09-17 — Quiet Architecture direction accepted; companion drafts

Owner approved the Dashboard's visual direction ("Ok đẹp đó") and requested
Trends and Devices. This accepts the direction for design development, not a
production implementation or every inferred behavior. Earlier NOT approved
entries below describe their status when created; this entry supersedes that
status for the Quiet Architecture Dashboard direction only.

- [Trends draft](drafts/2026-09-17-pro-max/trends-quiet-architecture.png): same
  header/top navigation; four vertical metric selectors and one large chart.
  Selected CO2 tile is forest green; the chart uses dark green on porcelain,
  readable relative-time/ppm axes, and Current/Min/Max. Last 15 min is the
  illustrated window, not authorization for persistence or new range controls.
  Sample current/min/max = 420/420/780 ppm; graph is illustrative, not telemetry.
- [Devices draft](drafts/2026-09-17-pro-max/devices-quiet-architecture.png):
  four equal cards with appliance, current mode, output switch and a separate
  Settings action row. Auto cards identify VOC/CO2 as the controlling sensor.
  Two ON outputs match Overview (purifier/light); fan/humidifier are OFF.
  Mode is distinct from relay state. Settings opens per-device configuration;
  the threshold editor itself has not been drawn in this delivery.

Preserve existing Auto/manual-override semantics in any later implementation:
an output-switch override must update mode appropriately, not silently leave
Auto controlling the output. Preset/polarity and LCD-editable thresholds remain
in per-device settings. No new scenes, scheduling, nodes or sensor driver scope.
Draft Auto modes are illustrative, not a change to startup defaults.

ui-ux-pro-max supplied a verified time-series line-chart match. Broad generated
marketing/glass recommendations were not substituted for the accepted style.
The progressive-disclosure query and one retry were off-topic; keeping settings
separate is a general UX/established-project fallback, NOT a verified search
match. Applied consistent navigation, icon language and hierarchy checklist.

Both images were visually inspected for core content, sample states and style
consistency. They are imagegen concepts, not native layout or interaction tests.
Font sizes, target contrast, touch areas, chart sampling and modal/error/offline
states need validation when prototyped. Secondary gray text may need darkening
on the physical LCD; do not treat raster shading or coordinates as exact tokens.
Production code, existing prototype and CURRENT fix handoff are unchanged.

## 2026-09-17 — Free design exploration: Quiet Architecture (NOT approved)

Owner explicitly released previous visual preferences/constraints for this
design trial. Do not constrain new concepts to the old palette, icon colors,
four separate metric cards, bottom icon-only navigation or borderless flat
surfaces. This is concept freedom, not approval to replace production code.

[Quiet Architecture concept](drafts/2026-09-17-pro-max/dashboard-quiet-architecture.png)
reorganizes the Dashboard around a prominent air-status panel, two supporting
comfort metrics and one aligned device-control list. Labelled navigation moves
to the top. Warm stone/porcelain, forest green and celadon replace the previous
rainbow tiles; typography and surface contrast establish hierarchy. Subtle
gradient, fine dividers and different icon treatment are exploratory choices.

Applied ui-ux-pro-max design-system search plus explicit `bento dashboard
hierarchy` style match (bento-box-grid); adapted asymmetric grouping, hierarchy,
spacing, coherent icon language and readable controls. Marketing sections from
the broad search are irrelevant to this appliance. LVGL stack guidance is not
available from the skill. No implementation or performance claim is inferred.

Air quality `Good` and Auto/manual device modes are illustrative sample states,
not a new aggregation algorithm or changed defaults. Any later implementation
must define combined air-status behavior for mixed/invalid/stale sensors and
validate all current pending/error/recovery states. The screenshot is an
imagegen concept, not measured native geometry, LCD contrast or working touch.
Production code, previous prototypes and CURRENT remain unchanged.

## 2026-09-17 — UI UX Pro Max exploration (NOT approved)

Owner requested a reusable global installation of `ui-ux-pro-max` and another
design trial. Latest scope remains Dashboard preview only; production remains
the existing design. Earlier blue/purple/pink requirements are no longer binding
for exploration. Keep bright surfaces, black content icons and borderless cards.

[Dashboard concept](drafts/2026-09-17-pro-max/dashboard-concept.png)
uses four equal metric columns and a two-by-two appliance grid. Equal value
baselines and consistent gaps aim to improve scanning; wider appliance rows
give names and toggles more room. Teal/blue/sand/aqua metric surfaces, dark ink,
restrained teal selected navigation and switches are proposals, not decisions.
Preserve uptime, Connected, CO2/VOC Good text, temperature/humidity units,
Manual modes, four switches and icon-only three-tab navigation. No new Room Air,
Details, notifications, Flash logging or automation semantics.

Skill application: initial `smart home dashboard minimal --design-system`
returned a dark/glass marketing pattern that conflicts with this product.
One narrower `minimalism functional touch dashboard --domain style` returned
`minimalism-and-swiss-style`; `touch target spacing --domain ux` returned spacing
and platform-specific target guidance. Adopt grid/contrast/consistent spacing;
do not import website CTAs, glass blur, mobile dp/pt as literal LCD pixels,
new fonts or motion libraries. LVGL is not among the skill's supported stacks.
Its general principles are adapted to the existing embedded constraints.

Concept geometry intent: 24px side gutters, 16px metric gaps, 4x176px cards,
two 368px appliance columns, >=44px intended touch height, >=18px text.
These are design targets, NOT measured claims about the generated bitmap.
Imagegen may introduce subtle shading/geometry variation; any implementation
must retain flat fills and pass native 800x480/readability/overflow/state checks.
This happy-path image does not validate Sending, Retry, offline, long values or
Auto paused states. Those existing behaviors must survive a later redesign.
No implementation authorization is inferred from this concept.

## 2026-09-17 — Keep existing design; stronger LCD surface colors

Owner retains the existing design; the premium raster mockup is not approved
for implementation. Only strengthen the four Home metric backgrounds, Trends
metric-selector backgrounds and Devices icon plates to reduce LCD washout.
Keep canvas, foregrounds, selected-state blue, layout, typography, shadows,
borderless cards and behavior unchanged. No gradients or new borders.

Implementation palette (RGB888 inputs, rendered as RGB565): CO2 #ADCEFF,
VOC #CEB5F7, temperature #FFD394, humidity #94D7C6; inactive Trends selectors
#BDD3EF, Devices icon plates #B5CEF7. Selected Trends remains #215DDE.
Native captures: [Home](reviews/2026-09-17-lcd-palette/home_good.png),
[Trends](reviews/2026-09-17-lcd-palette/trends_data.png),
[Devices](reviews/2026-09-17-lcd-palette/devices_grid.png).
Physical LCD/backlight appearance still needs owner confirmation.

## 2026-09-17 — MCU integration ownership; visual direction unchanged

Owner requests the motorCmd convention: the UI component defines all six LoRa
globals once in ui.c; firmware consumes header extern declarations and publishes
RX/TX state. Use plain lvgl.h includes. No UI layout, Auto policy or packet change.
See [integration guidance](MCU_BASELINE.md) and
[independent review](reviews/2026-09-17-r1-r4-independent/REVIEW.md).

## 2026-09-16 — X1–X4 review: visual direction and touch targets accepted

Independent capture/probes confirm68x44 switch targets and separated recovered
switch/Retry on Home/Devices. Keep current palette, metric cards and modal.
Remaining work concerns timing rollover, export safety and acceptance evidence,
not a visual redesign or a new Auto policy. See the
[result review](reviews/2026-09-16-auto-x1-x4-independent/REVIEW.md) and
[current Y1–Y3 handoff](prompts/CURRENT.md). Physical target checks remain open.

## 2026-09-16 — W1–W4 Review: Retain Layout & Finalize Touch Targets

Retry availability following reconnect / late ACK and physical switch readback display behavior have been verified.
Retain active color palettes, Temperature/Humidity metric cards, and modal layouts; no broad redesign.
Newly measured switch touch footprint was 54x30: finalize the >= 44px vertical height requirement by expanding the touch hit area without clipping or overlapping adjacent Retry/Settings buttons, while preserving visual pill dimensions.
This finalizes existing requirements without introducing new Auto engine behaviors or scope changes.
Refer to the [independent review](reviews/2026-09-16-auto-w1-w4-independent/REVIEW.md) and [prompt X1–X4](prompts/CURRENT.md). Target hardware bench validation on physical LCD/touch panel remains open.

## 2026-09-15 — Preserve UI Direction; Add Re-arm for Latched Error States

The V1–V4 review confirmed that preset dropdowns and timeout metadata groupings are improved; no redesign of cards, modals, colors, or typography is requested. Refer to [W1–W4 review](reviews/2026-09-15-auto-v1-v4-independent/REVIEW.md) and [active prompt](prompts/CURRENT.md).

Technical lead specification for W1: when online, if an Auto channel remains in `paused_error` while the command phase is IDLE due to link reconnection or late ACK, the Retry/re-arm button must remain accessible. Display true reported readback rather than artificial `Unknown` states. Render Row 1 as `"Auto paused"`, Row 2 with the reported switch on the left and Retry on the right, maintaining >= 44px touch target clearance; apply corresponding layout on Devices. This represents a local state variant for the latched recovery condition. Do not silently clear the latch or alter Auto policy.

## 2026-09-15 — Automatic Prompt Delivery Post-Review; Retain Stable UI Baseline

- **Owner Directive:** Every review finding remaining defects or unresolved issues must automatically produce a corrective fix prompt in the same turn without requiring a separate request. Stored exclusively at [prompts/CURRENT.md](prompts/CURRENT.md); archive superseded versions before replacing. Review turns do not grant authorization to implement code, commit, or push.
- **Technical Lead V1–V4 Direction:** Retain primary layout, Temperature/Humidity cards, and balanced settings geometry. Finalize preset selection popup (`ui_font_18`, row pitch >= 44px, bounded within modal container, borderless) and balance status grouping during timeout/offline conditions.
- For composite states, prioritize two metadata rows: mode (`Auto paused` or `Manual`) spanning full width on the top row; state (`Unknown`) alongside Retry/dash on the bottom row, avoiding cramped three-line baseline compression. Measure against font metrics and native screenshots; retain font size >= 18px, bottom clearance >= 10px, touch targets >= 44px, and preserve control logic.
- Details here serve as technical lead specifications for the subsequent fix cycle. Retain palette, modal dimensions, Auto policies, and packet schemas. Refer to [V1–V4 review](reviews/2026-09-15-auto-layout-result-review/REVIEW.md).

## 2026-09-15 — Auto Layout Balancing & Metric Card Proportions (Technical Lead Direction)

**Owner Directive:** Review submitted fixes; re-balance Auto settings, Temperature, and Humidity card spacing; authorize technical lead to decide necessary adjustments and incorporate into new prompt. Coordinates below were selected under this authorization.

- Retain Study 12 flat light palette, 4 metric color accents, subtle elevation shadows, and borderless card frames. Do not alter outer screen bounds, re-introduce gradients, tabs, or unauthorized Auto features.
- Modal dimensions fixed at (180, 56, 440, 368), padding 20px, content area 400x328px. Header height 44px; Save button anchored at (0, 280, 400, 48) relative to content origin (200, 76), remaining stationary across error states.
- Device settings arranged in horizontal rows: y=56/112/168/224, 44px per row, 12px vertical gaps. Label/control columns at x=0/140; dropdown width 260px; Manual/Auto buttons width 124px, gap 12px. Set limits spans full content width; polarity switch maintains >= 44px touch hit target.
- Auto thresholds: context header at y=56; ON/OFF rows at y=92/152, height 48px; aligned label, minus, value, plus columns. Reset limits at y=212, height 44px; fixed two-line error slot; Save button aligned identically with Device settings view. Value 10000 does not shift layout columns. No `(index)` suffix for VOC; retain 1..500 unitless range and established threshold tables.
- Temperature and Humidity cards fixed at 236x98px, 12px horizontal gap. Two text lines centered based on true font metrics: title `ui_font_18` at y=12 (line height 25px), value `ui_font_34` at y=41 (line height 46px), text margin x=44; 24px icons at x=12, y=37 relative to card origin. Vertical margins ~12px top / 11px bottom.
- Do not re-introduce "Ideal", "Comfortable", "Healthy" or arbitrary comfort bands lacking logic. Invalid telemetry displays an em-dash `—` with inline `No data` in the value row; lost link displays `Offline`. No tertiary footer rows; do not display stale readings next to dashes. Valid readings maintain inline units and stable positions.
- Home device status retains dedicated space separate from switch/Retry controls, with text >= 18px. `Auto` / `paused` may wrap into two lines when appropriate; verify Sending, sensor pause, offline, error, and recovery states across all four tiles.
- Link loss during Auto must indicate paused state; do not alter relay policies, treat link loss as a permanent error latch, or trigger autonomous retries upon reconnect.

Refer to [latest review](reviews/2026-09-15-auto-v1-layout-review/REVIEW.md) and [active handoff](prompts/CURRENT.md).

## 2026-09-15 — Resolution of R1–R10, Preserving Product Design

The owner directed that prompts resolve all independent review defects. Active handoff is [prompts/CURRENT.md](prompts/CURRENT.md); prior implementation prompts are archived. Retain LCD threshold editing and established threshold tables. Focus areas: Manual/Auto opt-in, TX/config/recovery lifecycles, draft state isolation, touch interaction, font tables, and readback verification.

## 2026-09-14 — LCD Threshold Editing & Prompt Conventions

- **Owner Decision:** Auto thresholds must be editable on the LCD, replacing the fixed-thresholds-only proposal. All implementation and fix handoffs must use standardized storage conventions.
- **Technical Lead Specification:** Retain 3 appliance rules: Ventilation Fan/CO₂, Air Purifier/VOC, Humidifier/RH; each appliance maintains independent ON/OFF thresholds. Baseline values serve as initial defaults. Editor resides inside existing settings modal, with validation, Reset limits, shared draft state, and a single Save commit point; no new navigation tabs.
- Detailed timing, manual overrides, fault masking, recovery sequences, configuration migration, and acceptance tests are specified in [CURRENT.md](prompts/CURRENT.md).

---

## Operating Guidelines & Working Agreements

- Initial design phases produce visual artifacts only: wireframes, mockups, or native 800x480 PNG renders.
- Interactive HTML/CSS prototypes in browsers serve to validate user flows and screen states; prototyping does not imply initiating C/LVGL implementation.
- C/LVGL source code modifications and heap/xSPI profiling are deferred until user flows and screen layouts are approved.
- Physical schematics serve as hardware interface references, not design constraints.
- Implementations proceed against [CURRENT.md](prompts/CURRENT.md); independent review evaluates code, simulator captures, unit tests, and memory metrics.

## Product Context Baseline

- Smart Hub: 800x480 capacitive touch screen, QSPI display with display-side GRAM, LoRa transceiver.
- Smart Node: Four 10A relays, shared RS-485 bus supporting two environmental sensors.
- Application: Exhibition demo for smart residential or studio workspace environments.
- Target Audience: Homeowners, office staff, and technology enthusiasts.
- Network Topology: Peer-to-peer with exactly one Smart Node; no multi-node management UI required.
- Core Inquiries Addressed by UI: "Is the indoor environment healthy?" and "Which appliances are currently running or reporting faults?".
- Trends Data: In-memory session logging only; no non-volatile Flash logging.
- Language & Units: English language interface, light theme, modern Smart Home aesthetic, °C temperature. Top bar displays running session uptime (`HH:MM:SS`), not time-of-day. Real-time timestamps follow 24-hour notation.

## Telemetry Presentation Decisions

### eCO₂ (updated 2026-09-25)
- **Decided (D-039):** Displayed on primary surfaces with label `eCO₂` and unit `ppm`, matching the `MXVC10S-B-P67` equivalent-CO₂ output. This supersedes D-002 (`CO₂` label).
- Valid display range 400..60000 ppm; other values are shown as unavailable.
- **Open:** Final alert color bands; the UI currently uses Good ≤ 800, Moderate ≤ 1200 ppm.

### TVOC (updated 2026-09-25)
- **Decided (D-039):** Primary cards show the raw TVOC concentration in `ppb` with the label `TVOC`, alongside `Good / Moderate / Poor`. This supersedes D-003 (unitless VOC Index).
- The Node sends ppb as read, without index conversion or clamping; values above 60000 ppb are shown as unavailable.
- Dedicated `Details` tab remains removed (Study 04).
- **Decided (D-042, 2026-09-28):** ppb cut-offs Good ≤ 220, Moderate ≤ 660, Poor above (UBA TVOC levels for SGP30-class sensors). All visible copy reads `TVOC`.

**Historical note:** Sensirion VOC Index 1–500 is a relative index computed from SGP4x raw signals via the Gas Index Algorithm, using recent history as an adaptive baseline (typical air around 100). There is no linear conversion between TVOC ppb and that index. This is why v2→v3 configuration migration resets purifier thresholds instead of converting them. Reference: [Sensirion VOC Index Note](https://sensirion.com/media/documents/02232963/6294E043/Info_Note_VOC_Index.pdf).

## Established Design Principles

- **Glance-First:** Assess room environment, node connectivity, and active relays within 5 seconds.
- Distinguish **reported physical state**, **in-flight command state**, and **automation logic**.
- Stale or severed telemetry must be clearly indicated (em-dashes and `Unknown`), never presenting frozen numbers as live data.
- Avoid color as the sole indicator; accompany with text, icons, and badges.
- Avoid technical jargon on primary operational screens.

## Information Architecture

Organized around a **room-first dashboard**:

1. `Home`: Hero air quality metrics (eCO₂, TVOC, temperature, humidity), active advisories, and four compact device tiles.
2. `Trends`: 16-point sliding history graphs across active session; resets upon system reboot. Live values appear on the four selector cards; the chart card shows only the graph, axes and availability badge (Current/Min/Max removed 2026-09-25).
3. `Devices`: Four relay cards displaying appliance presets, operational modes (Manual/Auto), state indicators, and configuration controls.

Navigation consists of three bottom tabs: Home, Trends, Devices. Notification panels and top-bar bell icons are omitted in favor of inline contextual advisories.

## Appliance Presets (Four Relays)

Configurable via preset selectors in the `Devices` tab:
- Air Purifier
- Ventilation Fan
- Humidifier
- Dehumidifier
- Heater
- Light
- Smart Socket
- Generic Device

Demo Configuration:
| Relay | Default Preset | Functional Role |
|---|---|---|
| 1 | Air Purifier | Automated actuation linked to TVOC (ppb) |
| 2 | Ventilation Fan | Automated actuation linked to eCO₂ (ppm) |
| 3 | Humidifier | Automated actuation linked to low humidity |
| 4 | Desk Light | Manual actuation for live demonstration |

Tiles render icon, appliance name, switch toggle, and `Manual/Auto` mode. Redundant `On/Off` text is omitted; switch position conveys state. Transient states display `Sending…`, `Unknown`, or `Retry`.

## Visual Studies & Design History

### Study 12 — Active Baseline: Flat Surfaces, Precise RGB565 Palettes
- Removed all CSS/LVGL gradients. Surfaces transitioned to flat fills: canvas (`#EFEFEF`), device cards (`#F7F7F7`), chart and dialog containers (`#FFFFFF`). Color accents reserved for metric tiles, active pills, and status badges.
- Colors mapped to exact 16-bit RGB565 bit patterns (`(v<<3)|(v>>2)` for R/B, `(v<<2)|(v>>4)` for G) to prevent rounding artifacts (e.g. pink tints on off-white surfaces).
- Technical rationale: LVGL v8.4 gradient caching uses keys derived from descriptor address and size without color attributes, causing color map collisions between cards of identical dimensions. Flat surfaces eliminate cache collisions and save LVGL heap.
- Splash background and embedded logo asset (`ui_splash_logo.c`) are synchronized with Warm Stone Canvas (`#F4F1EC`, RGB565 `0xF79D`), eliminating bounding seams and harmonizing with the Quiet Architecture canvas.

### Study 11 (Historical): Icon-Only Bottom Navigation
- Enlarged bottom navigation icons from 25 to 32px; removed text labels. Centered within 48px touch targets in a 64px navigation bar.
- Selected tab renders white icon on blue pill; inactive tabs use `#42546B`.

### Study 10 (Historical): Metric-First Layout & Separate Cards
- Split Room Air aggregate card into independent CO₂ and VOC hero cards.
- Screen grid: Top header 56px, content area 360px, navigation 64px; 760x336px grid with 250/250/236px columns and 12px gaps.
- Primary metric values use 62px typography; section labels 24px; comfort metrics 34/18px; status labels 18px.

### Study 09 (Historical): Elevation Shadows
- Introduced subtle drop shadow (`0 2px 10px 0 rgba(24,43,77,0.06)`) on primary cards without borders.

### Study 08 (Historical): High-Contrast Light Theme
- Near-white canvas (`#F7F8FA`), crisp white cards, high-contrast typography (`#182B4D` primary, `#42546B` secondary).

---

## Architectural Decision Log

| Date | ID | Decision |
|---|---|---|
| 2026-09-06 | D-001 | Discovery phase first; interactive drafts before C/LVGL implementation. |
| 2026-09-06 | D-002 | Primary label for eCO₂ is `CO₂`; disclosure notes omitted from primary view. |
| 2026-09-06 | D-003 | TVOC displays as unitless index; raw ppb excluded from hero cards. |
| 2026-09-07 | D-004 | Target application is residential/studio smart workplace demonstration. |
| 2026-09-07 | D-005 | P2P topology with a single Smart Node. |
| 2026-09-07 | D-006 | Home screen must communicate air status and relay states within 5 seconds. |
| 2026-09-07 | D-007 | Trends history maintained in RAM during session; no Flash logging. |
| 2026-09-07 | D-008 | English language UI, light aesthetic, °C temperature, 24-hour time. |
| 2026-09-07 | D-011 | Splash retains motion language from motor-control UI; uses Hyphen Deux mark. |
| 2026-09-07 | D-012 | Four default relays: Air Purifier, Ventilation Fan, Humidifier, Desk Light. |
| 2026-09-07 | D-013 | Home features quick toggle; detailed preset and Auto settings on Devices tab. |
| 2026-09-07 | D-014 | Three-tab navigation (Home, Trends, Devices); inline contextual advisories. |
| 2026-09-07 | D-021 | Top bar displays session uptime `HH:MM:SS`; link status displays Connected/Disconnected. |
| 2026-09-07 | D-026 | Enhanced typographic contrast; eliminated outer borders. |
| 2026-09-07 | D-033 | Eliminated gradients: all surfaces render flat RGB565 fills. |
| 2026-09-07 | D-034 | Pure neutral grey/white surfaces (`#EFEFEF`, `#F7F7F7`, `#FFFFFF`) replacing blue tints. |
| 2026-09-09 | D-035 | Adopted `motor_comm` direct global variable wire model (`lora_comm.h`). Eliminated `ui_snapshot_t` and push APIs. Node functions as dumb GPIO actuator; Hub resolves polarities (`desired_on ^ active_low`) and maintains local configs. |
| 2026-09-09 | D-036 | Replaced top-bar radio icon with 4-bar stepped indicator driven by SNR margin over demodulation limit (SF7..SF12: -7.5 dB to -20 dB). 1.0 dB hysteresis prevents border flicker. `Disconnected` indicated strictly upon packet timeout (5000 ms). Active color: `UI_COLOR_GREEN` (`#197047`), inactive: `UI_COLOR_SWITCH_OFF` (`#BDC7D6`). |
| 2026-09-14 | D-037 | Hub-side Auto v1 automation module supporting 3 appliances (Fan/CO₂, Purifier/VOC, Humidifier/RH) with LCD-editable thresholds in a 440x368 modal subview. Features 3-frame qualification, 10s anti-hunting hold, sensor fault masking, instant manual override, error latching (`Auto paused`), and safe re-arm. |
| 2026-09-25 | D-038 | LoRa protocol v2 (`0x02`): Hub-driven request/response. `GET_STATUS` (`0x03`) polls ~1 Hz; `HUB_CMD` only for pending commands (≤ 3 attempts). The Node replies only to requests. `sample_seq` gates Auto qualification; a decrease in `uptime_sec` triggers re-synchronization. `lora_last_rssi` is `int16_t`. Frame sizes unchanged. |
| 2026-09-25 | D-039 | Supersedes D-002/D-003: labels `eCO₂` (ppm) and `TVOC` (ppb). Purifier thresholds in ppb; config schema v3 resets v2 purifier thresholds to 150/100 ppb. |
| 2026-09-28 | D-042 | TVOC ppb bands Good ≤ 220 / Moderate ≤ 660 / Poor > 660 (UBA levels via Sensirion SGP30-class guidance; the MXVC10S manual has no bands); remaining `VOC` copy renamed `TVOC`. Purifier Auto defaults unchanged. |
| 2026-09-26 | D-041 | Trends current minute follows the latest valid reading (not the first sample of the minute); a closed minute keeps its last reading. Fixes the chart lagging up to 60 s behind the live value on all four metrics. |
| 2026-09-25 | D-040 | Trends simplified and refined by owner: restored hero forest card indicator with celadon dot, touch-down selection with 150 ms settle, native chart on normalized 0..1000 range, lowered title and chart (title y=14, chart y=136), raised unit label (y=96), fixed 15-min X-axis window (-15 min..Now), and round-number Y ticks (3 labels). Current/Min/Max, span label, tooltip, and area shading removed. |
