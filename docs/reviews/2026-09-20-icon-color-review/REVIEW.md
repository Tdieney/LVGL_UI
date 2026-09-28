# Quiet Architecture — independent icon, color and affected-flow review

> Owner clarification after this review: the icon complaint is about appearance
> (shape, curves, construction, internal details and stroke), NOT overall size.
> The size table below is secondary evidence and must not be treated as the fix.
> Color is being judged on sim_pc only; the UI has not been deployed to the LCD
> for this comparison. No panel/backlight/gamma explanation applies to the observed
> mismatch. Software RGB565 is still enabled inside sim_pc. The amended CURRENT
> requires shape-normalized comparisons and separates palette/rendering from
> quantization. This note does not re-review implementation changes made since
> the source hashes recorded below.

Date: 2026-09-20. Verdict: **CHANGES REQUIRED**, one consolidated handoff in
[CURRENT](../../prompts/CURRENT.md). Review only: no production code edits,
commit, push or regeneration of production icons/fonts in this review.

## Outcome

The implementation is closer and the ordinary Auto/editor flows are substantially
better. RGB565 is **not** the main explanation for the remaining icon mismatch.
Canvas and celadon are already close to the dashboard reference. The forest hero
is darker, less blue and completely flat compared with the concept. Its source
palette and rendering can be improved without changing display format.

There is also a limitation in the original design deliverable: the seven accepted
images are raster concepts, not a consistent vector asset/specification package.
For example, the purifier has one perforation row in Devices but three in the VOC
editor. One shared icon cannot be pixel-identical to both. Repeatedly asking AI to
redraw “exactly like the design” without a canonical source is not deterministic.
Choose one reference per icon, trace/correct it once, then freeze and reuse it.

Use [three-way comparison](COMPARE.html): original / original with RGB565-only
quantization / actual native framebuffer. The middle column is a browser sRGB
filter, NOT an LVGL render or a physical-panel simulation. It isolates color
quantization without changing icon geometry, typeface or composition.

## Scope and verification

Reviewed the current dirty working tree, latest Sep20 development claims, active
handoff, generated assets/font pipeline, theme, shell/pages/editors, affected
history and Auto command paths, simulator assertions and production verification
helpers. The Git diff includes earlier review rounds, not just the last icon edit.
Production source/asset hashes still match the latest submission's identity file;
the problem is not accidentally reviewing a different executable.

| Check | Independent result |
|---|---|
| Simulator build | PASS; upstream CMake developer warning only; [log](BUILD.txt) |
| Regression | PASS 33/33; [actual passing log](REGRESSION_PASS.txt) |
| Export manifest | PASS |
| Isolated production verification | PASS all four phases outside sandbox, including exit1 negative control |
| First production attempt in sandbox | Blocked by taskkill access denial, NOT established production-code failure; diagnostic fixture retained by tool |
| Review-pack controlled regression failure | PASS: exit42 converted to RuntimeError; helper test exits0. `REGRESSION.txt` here is that deliberately failing subprocess, NOT the passing suite |
| Independent boundary/recovery probe | 13 previous captures plus4 new chart/state captures; [source](probe.c), [log](PROBE.txt) |
| Standard captures | Fresh27 native800x480 captures; no edits to production fixtures/defaults |
| Heap stress | CHECK31 settled minimum5360B; observed cycle10→50 drift+32B; not a zero-leak proof or full MCU memory map |
| Physical LCD/touch/QSPI/firmware timing | Not tested; no panel photo or target measurement supplied |

The previous 10000ppm stats overlap, late-ACK switch/name collision, Settings
10000/9950 summary collision and pending-polarity Save error overlap did not
reproduce in the independent captures. Preserve these fixes. Home uses ellipsis
in the narrow recovered-latch name lane; that is not a newly discovered collision.
Two consecutive samples across midnight now show23:59→00:00 and Last1min.
Do not reopen these already repaired cases just to create another fix round.

## Remaining findings

### C1 — P2: icon scale, proportions and source-of-truth mismatch

Evidence: `tools/generate_assets.py:18` onward contains manually specified24×24
SVG paths; `ui_icons.c` contains46 A4 masks. Home uses32px canvases and Devices44px
canvases (`ui.c:1004`, `ui.c:1847`). Canvas size is not visible ink size.

| Devices icon | Original ink at800×480, approximately | Current A4 ink, alpha>=8/15 |
|---|---:|---:|
| Purifier | 36×51 | 24×34 |
| Fan | 42×49 | 34×34 |
| Desk light | 36×50 | 34×35 |

Reference measurement thresholds dark RGB pixels, so allow edge-AA tolerance;
these are not identical measurement domains or claims of subpixel precision.
The differences are large enough to explain the perceived mismatch. Fan is too
square, purifier too small, lamp too short/wide. The original is not restored
“pixel-for-pixel,” contrary to Sep20 DEV_LOG claims. Appliance icons are also
recolored forest instead of the reference's near-black ink.

RGB565 cannot cause these shape/aspect-ratio differences. A4 means16 opacity
levels, separate from framebuffer color depth. Native-size rasterization and
16 alpha levels can affect edges, but do not explain the wrong silhouette/scale.
Supersampling at build time can improve edge coverage; switching to A8 alone
cannot repair a wrong path. A44×44 A4 mask is968B versus1936B A8, before descriptor.

Use one canonical source for every role (Devices for appliances, Dashboard for
header/hero, Settings/Auto for controls), explicit optical bounds and shared
tokenized recoloring. Preserve coherent variants where they already match.
Do not crop colored rectangular screenshots into UI assets or use MCU SVG parsing.

### C2 — P2: forest palette/depth mismatch is mostly avoidable

Measured blank regions of original Dashboard versus actual native capture:

| Region | Reference median | Actual RGB565 decoded to RGB888 |
|---|---|---|
| Canvas | #EEECE7 | #EFEBE7 |
| Porcelain card | #F3F1EC | #F7F7EF |
| Hero upper region | #23443B | #183829 |
| Hero lower region | #19372F | #183829 |
| Active celadon tab | #D7E0BC | #D6DFBD |

The canvas/celadon are close; a global saturation/brightness redesign is not
warranted. Porcelain is brighter and slightly more yellow/green. Hero loses the
lighter/teal upper tone and all depth. `ui_theme.c:74` sets one opaque forest
fill; no gradient is configured. This is not evidence of a tested RGB565 limit.

Current source `#1D3B2E` becomes `#183829` after LVGL's bit truncation and our
bit-replication decode. Both the chosen source color and quantization contribute.
Representable example endpoints `#214539` and `#183831` are already nearer the
measured upper/lower regions; they are implementation candidates, not a newly
owner-approved palette. Native framebuffer patches, not source HEX alone, decide.
Keep canvas/celadon near present appearance; tune surfaces and near-black icon
ink with the same measurement method. Preserve readable secondary text.

RGB565 has32 red,64 green,32 blue levels. Quantization may shift near-neutrals
and band shallow gradients. Across original Dashboard, RGB565-only mean absolute
8-bit channel errors are approximately2.46/1.69/2.12. This is diagnostic, NOT a
perceptual similarity/acceptance score. See [measurements](MEASUREMENTS.json) and
[read-only analysis](analyze.py) for regions, formulas, icons and hashes.

Local LVGL8.4 `src/misc/lv_color.h:150` confirms R/B>>3,G>>2. Official references:
[LVGL8.4 Colors](https://lvgl.io/docs/open/8.4/overview/color) and
[LVGL8.4 Images](https://lvgl.io/docs/open/8.4/overview/image).
Simulator is depth16, swap0; captures decode little-endian correctly. No evidence
of an RGB/BGR or byte-order error here. Target gamma/backlight/viewing-angle and
driver byte order require physical validation; do not change them speculatively.

### C3 — P1: Trends units become nearly invisible after metric changes

`update_trends_tabs()` (`ui.c:1239`) changes names, values and icons but never
`s_trends_tab_units`. Units retain their initial foreground when selection changes.
CO2→Temperature/Humidity leaves white `ppm` on porcelain; another selected metric
keeps dark-muted units on forest. Reproduced in
[inactive units](probe_trends_units_inactive.png) and [negative-temperature view](probe_trends_negative.png).
Measured contrast: white/surface1.08:1; muted/forest2.10:1. Ordinary muted/surface
is5.66:1, so this is a state-color bug, not inherently too-small text or RGB565.
Update all role colors together and test every selection transition/return.

### C4 — P2: missing-data chart semantics and point rendering remain incorrect

- With16 timestamped slots10:00..10:15 and only indices2..13 valid, chart still
  positions samples over all16 slots but labels its endpoints10:02..10:13 from
  valid samples. First valid point is inset although left tick says10:02.
  [Reproduction](probe_trends_edge_gaps.png); `ui.c:1629,1708,1717,1741`.
- A sole valid point at index8 reports Current420/Min500/Max500 and “Single
  reading” but no point is drawn; its10:08 label is incorrectly at the left edge.
  [Reproduction](probe_trends_middle_singleton.png).
- `on_chart_draw_part_begin()` assumes line and marker callbacks are separate.
  In LVGL8.4 line drawing, p1/p2 and rect_dsc coexist; the `else if` marker path
  misses intermediate valid pairs. All nominal dots remain equal; isolated
  readings adjacent to NONE can disappear; last-index highlight is not last
  **valid** point. Inspect `src/extra/widgets/chart/lv_chart.c:1130` onward.
  Adjacent translucent area polygons also leave darker shared-edge bands.

Keep truthful values and gaps. Fix domain/timestamp mapping and isolated-point
visibility, then subordinate intermediate dots and emphasize endpoint/inspection.
Do not bridge missing intervals or invent smoothing/history samples.

### C5 — P2: existing hierarchy target not implemented; report claims otherwise

Navigation labels are `ui_font_18` at `ui.c:797`, not24 as CURRENT/report claim.
They meet the minimum18 floor; this is a hierarchy/acceptance discrepancy, not a
minimum-size violation. Hero CO2/VOC readings are34, much less prominent than the
reference, and Auto numbers and units share34 instead of distinct hierarchy.
Keep the current Plus Jakarta Sans and accepted layout; finish the already
requested role-based sizing with measured long-value fallback and disjoint lanes.
Do not globally enlarge every caption or undo repaired maximum-value layouts.

### C6 — P2/P3: proof and reproducibility claims need correction

- CHECK33 mostly asserts font/object containment, not the correctness of time
  labels, visible plot points or color-state changes. All17 independent probe
  captures pass that audit, including the three confirmed C3/C4 defects.
  Label-object rectangles are not proof of unclipped actual glyph ink.
- The icon generator says “exist and verified” when it only checks existence
  (`tools/generate_assets.py:164`). The review contact sheet re-renders SVG in
  Edge, not the compiled A4 arrays. Neither proves checked-in masks match source.
  Pin canonical source, role sizes, font file/weight and build dependencies; fail
  clearly on missing required fonts instead of silently changing family. Include
  font source/license provenance with the bundled TTF; do not re-license it.
- Actual host objects: icons21120B, fonts100336B. Report/MCU_BASELINE still say
  40icons17920B and Segoe88240B; actual family is46icons and Plus Jakarta Sans.
  Their old color and Study12-only/no-gradients statements also remain active-looking.
  Historical prototype is not Quiet Architecture; stop using it as visual authority.
- Production tests pass, but watchdog identity bookkeeping is PID-only;
  `Terminate-OwnedProcessTree()` can call taskkill even after a tracked worker
  has exited. A reused PID is not proof of ownership. Enumeration errors are
  swallowed and synchronous stream Result waits sit outside elapsed-time guard.
  These are remaining scoped safety/proof gaps, NOT an observed unrelated
  process termination. Preserve passing tests; use process identity/owned handles
  and bounded stream draining, with safe disposable negative fixtures.

Do not claim all C1–C6 closed on33/33 alone. Add focused assertions and one final
matching-fixture capture pack from the final executable. Exact photographic
texture and perfectly identical original AI raster edges are not blockers.

## Handoff

[ONE CURRENT prompt](../../prompts/CURRENT.md) includes all known corrections,
preserves the Auto/wire/MCU contract, and requires implementation self-review and
final regression in the same delivery. No extra owner-mediated fix phases.

Review method used the repo-local ui-ux-pro-max skill: optical rather than canvas
icon sizing, consistent family/roles, contrast by actual state, typography
hierarchy and stable geometry. It did not authorize a new visual design direction.
