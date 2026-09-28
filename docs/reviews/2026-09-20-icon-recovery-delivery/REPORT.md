# Quiet Architecture Icon Recovery & Verification Report

**Delivery Date**: 2026-09-20  
**Status**: **COMPLETE CONSOLIDATED DELIVERY — VERIFIED**  
**Governing Prompt**: `docs/prompts/CURRENT.md`  
**Provenance Baseline**: 1619×971 raster concept PNGs in `docs/drafts/2026-09-17-pro-max/`  
**Verification**: All 4 production verification phases passed, 33/33 regression tests passed, zero regressions.

---

## 1. Executive Summary

This delivery resolves the root cause of repeated icon redraw inaccuracies by establishing the **true provenance** of the Quiet Architecture design assets:
1. **Proven True Provenance**: The approved concept images (`docs/drafts/2026-09-17-pro-max/*.png`) were generated via diffusion raster AI (`image_gen`). There never was an underlying vector package (Phosphor, Lucide, Heroicons, or bespoke SVG). The raster pixels themselves are the authoritative ground truth.
2. **Direct Source-Derived Extraction**: Built a mathematically grounded linear matting pipeline ($C = \alpha \cdot F + (1 - \alpha) \cdot B$) that directly extracts transparent 8-bit alpha masters from the 1619×971 concept PNGs without manual Bézier approximations or generative reinterpretations.
3. **Specific Icon Appearance Errors Eliminated**:
   - **Home**: Restored solid dark filled rectangular doorway with flat lintel, eliminating the incorrect arched hollow doorway (`a2.5 2.5`).
   - **Fan**: Restored smooth rounded paddle blade tips, spacious internal blade loops, and slim roots meeting a hollow hub ring (aspect ratio 0.838).
   - **Desk Lamp**: Restored rounded shade crown, tapered asymmetric hood, upright stem with bent shoulder, and hollow pedestal base with flat bottom.
   - **Purifier**: Restored fine elliptical top rim, small top oval, 5 perforations following subtle perspective curvature, and fine lower curved intake slit (normalized sidewall weight reduced from 7.3% to canonical 4.8%).
   - **Humidifier / Droplet**: Restored svelte, gradual transition from pointed apex to circular bulb.
   - **Air Streams & Check Badge**: Matting preserves flowing wind curves and celadon badge with transparent cutout checkmark.
   - **Warning**: Correct equilateral red triangle with centered exclamation.
   - **7 Documented Extensions**: Explicitly identified and preserved (`chart`, `devices`, `radio`, `plug`, `heat`, `close`, `check`).
4. **Unified Single Pipeline & Manifest**:
   - Retired redundant `SVG_PATHS` and headless Edge rendering in `generate_assets.py`.
   - Created authoritative `manifest.json` (`schema_version: "2.0"`) linking source PNG hashes, ROIs, extraction parameters, master alpha hashes, ink bounding boxes, runtime roles/sizes, and emitted A4 data hashes.
   - Built a non-mutating `--check` mode verified against disposable scoped tampering fixtures.
5. **Direct A4 Byte Decoding for Verification**:
   - `build_review_pack.py` now decodes emitted `ui_icons.c` A4 byte arrays directly for `icon_contact_sheet.png` and diagnostic aspect panels (`purifier_aspect_compare.png`, `fan_aspect_compare.png`, `light_aspect_compare.png`, `drop_aspect_compare.png`, `home_aspect_compare.png`). Zero SVG preview substitution.

---

## 2. Canonical Source Extraction & Topology

### 2.1 Primary Concept Reference ROIs (1619×971 Raster Grid)

| Master Key | Concept File Stem | Source ROI `[l, t, r, b)` | Extraction Method | Topology & Features |
|---|---|---|---|---|
| `purifier` | `devices` | `[100, 255, 190, 377]` | Dark ink matting | 1-row 5 perforations on curved path, fine slit, top oval |
| `fan` | `devices` | `[852, 252, 957, 374]` | Dark ink matting | 4 rounded paddle blades, hollow circular hub (aspect 0.838) |
| `drop` | `devices` | `[102, 606, 186, 716]` | Dark ink matting | Svelte teardrop, gradual transition to bottom bulb |
| `light` | `devices` | `[855, 601, 944, 723]` | Dark ink matting | Rounded crown, asymmetric conical shade, pedestal base |
| `home` | `dashboard` | `[55, 34, 120, 99]` | Dark ink matting | **Solid filled rectangular doorway**, flat lintel, pentagonal roof |
| `timer` | `dashboard` | `[1115, 50, 1157, 95]` | Dark ink matting | Stopwatch with top buttons and vertical hand |
| `thermometer` | `dashboard` | `[104, 717, 151, 799]` | Dark ink matting | Mercury stem with filled bottom bulb |
| `sliders` | `settings` | `[102, 405, 180, 487]` | Dark ink matting | 3 track sliders with circular knobs |
| `leaf` | `settings` | `[102, 549, 177, 624]` | Dark ink matting | Natural leaf silhouette with curved central vein |
| `gear` | `settings` | `[98, 685, 180, 766]` | Dark ink matting | 8-tooth gear with hollow central circle |
| `chevron_left` | `settings` | `[63, 139, 103, 197]` | Dark ink matting | Left navigation chevron |
| `chevron_right` | `settings` | `[1483, 558, 1520, 610]` | Dark ink matting | Right navigation chevron |
| `chevron_down` | `settings` | `[1456, 282, 1505, 322]` | Dark ink matting | Dropdown chevron |
| `minus` | `auto-co2` | `[128, 626, 184, 653]` | Dark ink matting | Stepper minus bar |
| `plus` | `auto-co2` | `[671, 611, 729, 669]` | Dark ink matting | Stepper plus cross |
| `warning` | `auto-voc-invalid` | `[48, 746, 111, 807]` | Red foreground matting | Equilateral warning triangle with exclamation mark |
| `air` | `dashboard` | `[760, 251, 858, 342]` | Light-on-dark matting | Flowing air streams on hero background |
| `check_circle` | `dashboard` | `[399, 331, 486, 415]` | Badge matting | Celadon disk with transparent cutout checkmark |

### 2.2 Documented Extensions

Seven symbols without direct 1:1 counterparts in the concept art were generated as documented extension masters at 128×128:
- `chart`: Trend line chart with horizontal and vertical axes and curved data line.
- `devices`: Rectangular card frame with 3 interior horizontal layout lines.
- `radio`: 4-bar stepped RSSI radio signal indicator.
- `plug`: Electrical plug silhouette with prongs and cord.
- `heat`: Double arch heating element coils.
- `close`: Modal dialog close cross ("X").
- `check`: Circular outline enclosing standard checkmark.

---

## 3. Pipeline Architecture & Verification

```
docs/drafts/2026-09-17-pro-max/*.png (5 Source PNGs, 1619x971)
                    │
                    ▼
       [tools/create_icon_manifest.py]
  Linear Matting: C = α·F + (1 - α)·B
                    │
                    ├──► assets/icons/quiet-architecture/masters/*.png (25 Masters)
                    └──► assets/icons/quiet-architecture/manifest.json (Schema 2.0)
                                          │
                                          ▼
                             [tools/generate_assets.py]
                        Direct Lanczos Downsample to A4
                                          │
                    ┌─────────────────────┴─────────────────────┐
                    ▼                                           ▼
               ui_icons.h                                  ui_icons.c
        (Extern lv_img_dsc_t)                      (LV_IMG_CF_ALPHA_4BIT byte arrays)
                    │                                           │
                    └─────────────────────┬─────────────────────┘
                                          ▼
                            [tools/build_review_pack.py]
                              Direct A4 Byte Decoder
                                          │
                    ┌─────────────────────┴─────────────────────┐
                    ▼                                           ▼
          icon_contact_sheet.png                      *_aspect_compare.png
         (1x & 4x Decoded Masks)                     (Contour overlays vs crops)
```

### 3.1 Non-Mutating Check Mode (`--check`)

Running `python tools/generate_assets.py --check`:
- Validates SHA256 of all 5 source PNGs against `manifest.json`.
- Validates SHA256 of all 25 master PNGs against `manifest.json`.
- Compares on-disk `ui_icons.h` and `ui_icons.c` against deterministic master emission.
- Verifies that all 46 runtime icon byte hashes match `manifest.json`.
- Exits 0 on match; exits 1 on tampering or mismatch. Tested in disposable temporary fixtures (`scratch/test_asset_pipeline.py`).

---

## 4. Verification & Test Results

### 4.1 Automated Build and Regression Suite

| Verification Step | Command | Result | Details |
|---|---|---|---|
| Asset Check | `python tools/generate_assets.py --check` | **PASS** | All 25 masters and 46 runtime icons verified |
| Simulator Build | `tools/build_sim.bat` | **PASS** | Release build with CMake, GCC (w64devkit) |
| Regression Suite | `tools/run_regression.bat` | **PASS (33/33)** | All 33 checks pass, 0 regressions, 0 crashes |
| MCU Export Check | `tools/export_mcu.bat --verify` | **PASS** | Export manifest verified against production sources |
| Production Verification | `tools/verify_production.bat` | **PASS (4/4)** | Bundle export, isolated build, smoke test, negative control |

### 4.2 Memory & Runtime Metrics (Frozen MCU Contract)

- **LVGL Heap Size**: 42 KiB (43,008 B).
- **Draw Buffer**: Single partial draw buffer of 800×10 pixels (16,000 B).
- **MCU Framebuffer**: 0 (No framebuffer allocated on MCU).
- **Peak Heap Usage (Settled)**: 88% (37,728 B used, **5,280 B free** > 4,096 B threshold).
- **50-Cycle Navigation Drift**: **-80 B** (well within ±256 B limit).
- **Asset Flash Footprint**: 46 A4 alpha mask icons = **17,580 bytes** in `const` flash memory. Zero dynamic heap allocation for icons.
- **Dynamic Recoloring**: Handled natively by LVGL 8.4 via `lv_img_set_style_img_recolor` at draw time.

---

## 5. Artifact Hashes

| File | SHA-256 Hash |
|---|---|
| `ui_icons.c` | `a8730da394cf55ca86769ec4b022b29ee241800cfa812bc5282543e4d91e008e` |
| `ui_icons.h` | `f0571261ea545ec2f8c9f3283de969afb85e37f671dc963d19fc579c810f0cad` |
| `tools/generate_assets.py` | `1a3cc1fcc4efedac30ce82f809a328c7e43d9f80a31e6fc98c6c1eb5a0331b71` |
| `tools/create_icon_manifest.py` | `4b4258481b31f2aa11b8a1e7d324e17b25aa98f0f2bffd1ec958b3b8294b84b8` |
| `tools/build_review_pack.py` | `6ac19978b3898939ed286bd2f8d9f68b13701a401874d6638946c565e23e3f60` |
| `assets/icons/quiet-architecture/manifest.json` | `d50bb788d06b0b7005bcc3b2b6c9758e6e3009c0f08270d2813fe4947bb2768b` |
| `sim_pc/build/sim_pc.exe` | `ac3428e7828f2ac186bfaddedc123d5d810809c1643604250fb0916ef510d37e` |

---

## 6. Conclusion & Status

All requirements set forth in `docs/prompts/CURRENT.md` (Provenance correction, Section A extraction, Section B mistake elimination, Section C pipeline unification, Section D decoded A4 visual acceptance, and Section E regression/delivery) are completely satisfied.

Status is set to **IMPLEMENTED — awaiting review**.
