# Quiet Architecture icon provenance and appearance review

2026-09-20 — targeted investigation requested by owner. **CHANGES REQUIRED** for
icon appearance and its generation/verification pipeline. This is NOT a new
full functional acceptance review of the intervening C1–C6 implementation.
No production source, generated UI asset, simulator behavior or palette changed.

## What actually happened during the original design

The owner is correct that ui-ux-pro-max was used. Its role was design guidance,
not delivery of the exact pictograms in the accepted pictures.

| Evidence | What it establishes |
|---|---|
| DEV_LOG Sep17 global-skill entry | Global skill installed; local style/UX searches; unsuitable automatic dark/glass proposal rejected in favor of suitable grid/minimalism/touch guidance |
| [Original Quiet Dashboard prompt](../../prompts/archive/2026-09-17-04-design-quiet-architecture.md) | Built-in image_gen, NO input image. Describes consistent outline appliance icons but names no icon package, asset URL, glyph ID, SVG or path |
| [Trends/Devices prompts](../../prompts/archive/2026-09-17-05-design-quiet-companions.md) | Each companion generated with image_gen and the approved Dashboard image as reference |
| [Settings/Auto prompts](../../prompts/archive/2026-09-17-06-design-quiet-settings-auto.md) | Settings/CO2 generated using Devices style reference; humidity/invalid VOC are edits of the CO2 raster |
| DEV_LOG Sep17 design entries | These turns wrote draft images/docs, not production/prototype code |

The prompt specifies “Plus Jakarta Sans-like,” not a font-file-perfect rendering,
and “consistent outline icons,” not Phosphor/Lucide/Heroicons assets. The current
skill recommends icon libraries in general, but that does not establish which
library an image generator used internally. No exact original vector library or
recoverable author SVG is evidenced by this saved generation record.

Thus the accepted PNGs ARE the surviving icon appearance source. Current SVGs
under assets/icons/quiet-architecture are later reconstructions, not recovered
original design files. Claims of exact extraction/pixel-perfect restoration in
later DEV_LOG/report entries contradict both the paths and visible comparisons.

Technical-lead correction: earlier handoffs over-relied on prose descriptions and
insisted on canonical SVG reconstruction. That left the coding agent guessing
Bezier control points repeatedly. For this fixed-size LVGL UI, source-derived
transparent alpha masks are a valid alternative and directly preserve the
approved contours; another semantic redraw is not necessary.

## Concrete current discrepancies, independent of displayed size

[COMPARE.html](COMPARE.html) decodes checked-in A4 bytes from ui_icons.c and shows
them beside original raster crops at equal dark-ink height, preserving aspect
ratio. No SVG preview is substituted for the A4 data. The comparison is diagnostic
asset rendering, not a fresh screenshot of a running simulator.

| Icon | Original reference | Current reconstruction |
|---|---|---|
| Home | Rectangular, dark-filled doorway; flat lintel; pentagonal outer contour | Hollow arched doorway: literal `a2.5 2.5` arc in home.svg and generator |
| Fan | Rounded paddle ends, spacious hollow blades, slim roots meeting a hollow hub | Pointed ends/cusps, thicker contours, small pinched internal spaces; matching four blades alone is insufficient |
| Lamp | Rounded crown of shade; tapered asymmetric hood; upright stem with bent shoulder; pedestal with flat bottom and rounded upper shoulders | Closed angular/triangular shade with sharp apex and a capsule-shaped base; different object construction |
| Purifier | Fine elliptical rim, small top oval, five small holes following a slight curve, fine curved lower slit | Heavy rim/walls/button/slit; five holes placed on one flat Y coordinate; different interior balance |
| Droplet | Slender, gradual transition from pointed top to bulb | Broad shoulders and abrupt transition to a circular bulb, heavier outline |

For purifier only, a reproducible sidewall slice illustrates stroke rather than
screen-size error: reference5px wall /104px ink height ≈4.8%; current A4 wall4px /
55px ink height ≈7.3%. The normalized wall is about1.5× heavier. Sampling uses
dark RGB<120 versus A4>=8/15, so this is approximate, not an exact alpha metric.
See [source coordinates/hashes](SOURCE_CROPS.json) and [read-only analyzer](inspect_sources.py).
Home, lamp and fan construction differences remain even if stroke is reduced.

The submitted comparisons also show these differences:
[fan](../2026-09-20-quiet-architecture-consolidated/fan_aspect_compare.png),
[lamp](../2026-09-20-quiet-architecture-consolidated/light_aspect_compare.png),
[purifier](../2026-09-20-quiet-architecture-consolidated/purifier_aspect_compare.png).
Their capture exists; their text claiming the mismatch is closed is not supported.
Some submitted reference crops include neighboring label/leaf ink, contaminating
the overlay. The new source ROIs isolate the icons instead.

## Source coordinates — no guessing from an800x480 screenshot

All original files below currently measure1619×971. Coordinates are integer pixel
positions in those ORIGINAL PNGs, `[left, top, right, bottom)`. They are NOT LVGL
layout coordinates. Preserve these source hashes before generation; verify image
dimensions and visual content before use. Full18-role mapping is in SOURCE_CROPS.

| Icon | Original file stem, all end in `-quiet-architecture.png` | Clean source ROI |
|---|---|---|
| Purifier | devices | [100,255,190,377] |
| Fan | devices | [852,252,957,374] |
| Droplet | devices | [102,606,186,716] |
| Desk lamp | devices | [855,601,944,723] |
| House | dashboard | [55,34,120,99] |
| Timer | dashboard | [1115,50,1157,95] |
| Thermometer | dashboard | [104,717,151,799] |
| Sliders | settings | [102,405,180,487] |
| Leaf | settings | [102,549,177,624] |
| Gear | settings | [98,685,180,766] |
| Back chevron | settings | [63,139,103,197] |
| Forward chevron | settings | [1483,558,1520,610] |
| Dropdown chevron | settings | [1456,282,1505,322] |
| Minus | auto-co2 | [128,626,184,653] |
| Plus | auto-co2 | [671,611,729,669] |
| Warning | auto-voc-invalid | [48,746,111,807] |
| Air streams, light-on-dark | dashboard | [760,251,858,342] |
| Check badge, light-on-dark | dashboard | [399,331,486,415] |

Devices remains the canonical appliance source across pages, including its
one-row purifier. This is the technical-lead consistency choice already handed
off, NOT a claim the three-row VOC variant is identical. No repeated AI generation.
Heater/socket/unused navigation icons lacking an original counterpart must remain
documented extensions; do not invent source coordinates or call them extracted.
Signal bars are runtime state geometry; preserve their behavior.

## Pipeline defects making repeated redraws unreliable

1. `tools/generate_assets.py` consumes its own `SVG_PATHS` dictionary; it does not
   load canonical SVGs or manifest. `tools/create_icon_manifest.py` contains a
   second path dictionary and writes SVG files. Editing those SVGs alone cannot
   change production output. There is no single functioning source of truth.
2. Generator's default “exist and verified” branch only checks file existence.
   Changes to source are skipped unless --force-icons is supplied. This is not
   a source/output hash verification.
3. `tools/build_review_pack.py` contact sheet re-renders SVG with Edge. Its caption
   says A4 masks but it does not decode ui_icons.c; source-vs-generated differences
   can therefore escape the purported bitmap verification.
4. Manifest descriptions rely on semantic names/target sizes, not original image
   hash + verified crop + extracted contour. “Canonical” is currently a label,
   not proof of design provenance or generator input ownership.

## Corrective method and honest limits

The ONE [CURRENT](../../prompts/CURRENT.md) changes icon sourcing to measured
original-pixel masks. It specifies background removal, alpha preservation,
hole/fill topology, build-time aspect-preserving resampling and A4 output.
It supersedes SVG-only reconstruction, not the no-background-rectangle constraint.
If vectors are retained, they must be contour-fitted to the extracted mask and
pass the same source comparison, not reconstructed from words again.

Original PNGs contain composited shading/AA, not hidden vector paths or original
alpha. Exact author alpha recovery is underdetermined. Do not promise bit-perfect
matches to every shaded pixel. But rectangular versus arched doors, round versus
pointed fan tips and hood versus triangle are fixable source-geometry errors,
not RGB565 or physical LCD limitations. All color discussion remains sim_pc-only.

## Work and verification performed in this turn

- Read archived generation prompts, Sep17 design logs, current skill and latest
  submitted handoff; inspected original images and submitted comparisons.
- Inspected current asset/generator/manifest/review pipeline; decoded16 current
  A4 descriptors and measured original ROIs (plus2 light-on-dark source ROIs).
- Ran read-only inspect_sources.py; source/A4 byte sizes valid; recorded hashes.
- Rendered/visually inspected the diagnostic HTML in headless Edge. Initial
  sandbox attempt hit IPC access denial; permitted retry produced screenshot.
- No production edits, build/regression/target tests or new hardware conclusions.
  C2–C6 runtime closure claims from the intervening implementation are NOT newly
  verified here and are not automatically reopened by this icon-origin review.

ui-ux-pro-max informed stroke/outline/fill consistency and comparison discipline;
it did not provide the original icon geometry. The actual accepted raster controls
appearance when a generic skill recommendation would select a different icon.
