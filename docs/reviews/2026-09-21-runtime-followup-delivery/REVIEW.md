# Runtime/RAM follow-up delivery report — 2026-09-21

Disposition: **IMPLEMENTED & VERIFIED**, ready for final owner review.

## Executive Summary

This delivery resolves all remaining findings (**F1–F4**) from `docs/reviews/2026-09-21-runtime-followup/REVIEW.md` and `docs/prompts/CURRENT.md` in a single consolidated submission:

1. **F1 (Width-safe, stable Uptime Header layout):**
   - Implemented length-bucketed reservation model in `get_uptime_reserved_width(len)` (`8 chars -> 92px`, `9 chars -> 106px`, `10 chars -> 118px`, `len >= 11 -> len*13 - 12`).
   - The uptime label right-aligns dynamically within its stable reserved slot: `uptime_lbl_x = sep_x - 14 - uptime_w`, and the clock icon sits at `uptime_lbl_x - 28`.
   - Guaranteed $\ge 14\text{px}$ minimum clear gap to separator across all proportional font shifts.
   - Tested reconnect at narrow `111:11:11` (gap 59px), advance to wide `111:20:00` (gap 37px), `100:00:00` (gap 23px), `1000:00:00` (gap 22px), and `UINT32_MAX` (gap 47px).
   - Clock icon distance to uptime label left edge is strictly stationary at 8px. Zero jitter on second ticks.

2. **F2 (Exact unavailable-state copy simplification):**
   - **Home Temp/Humidity:** Removed "Offline" and "No data" notes (labels hidden and buffer empty). Displays em dash `"—"` and hides the unit label (`"°C"`, `"%"`).
   - **Home device rows & Devices cards:** Removed "Unknown" text. In offline/no-readback state, shows ONE dash; state text is hidden.
   - **Command error state:** Shows ONE dash `"—"` at the state label position alongside the "Retry" button. Dash widget is hidden to ensure zero pixel overlap.
   - **Trends:** Retained `"Data unavailable"` badge and `"No readings yet"` placeholder as specified.

3. **F3 (Durable regression assertions):**
   - Expanded `CHECK 34` in `sim_pc/main.c` with sub-tests `34g` through `34l`:
     - `34g`: Direct `lv_label_get_text` checks on actual numeric widgets (`"100"` + separate unit `"%"`, then next `"99"`).
     - `34h`: Rebuilt chart range verification (`ymin/ymax` match cache) and isolated points mapping.
     - `34i`: Full sensor availability matrix across Home, Trends, and Auto editors.
     - `34j`: Genuine no-op Save verification (callbacks count == 0, custom name preserved).
     - `34k`: Uptime proportional layout assertions (narrow reconnect `111:11:11` gap $\ge 14$, advance to `111:20:00` gap $\ge 14$, 100h, 1000h, `UINT32_MAX`, and stationary clock distance == 8px).
     - `34l`: Unavailable copy checks (no "Offline"/"No data" notes, no "Unknown", dash visible, error dash alongside Retry).

4. **F4 (RAM optimization & measurement):**
   - Pruned 24 unused styles from `ui_theme.h`/`ui_theme.c`, reclaiming 384 B of mutable BSS descriptors.
   - Converted all remaining 34 active shared styles in `ui_theme.c` to LVGL 8.4 compile-time `lv_style_const_prop_t` tables placed in Flash/rodata (`.prop1 = LV_STYLE_PROP_ANY`).
   - `ui_theme_init()` now allocates **0 heap bytes** at boot (down from 1,560 B before F4, and 2,808 B originally).
   - Settled Home free heap increased to **7,608 B** (+1,616 B headroom!).
   - Added `#ifndef UI_LVGL_HEAP_BYTES` and `#ifndef UI_DRAW_BUF_LINES` guards in `ui_mcu_profile.h` for clean firmware overriding.

---

## Verification Evidence

| Verification Gate | Result | Notes |
|---|---|---|
| `tools/build_sim.bat` | **PASS** | Builds cleanly with GCC `-Wall -Wextra -Werror`. |
| `tools/run_regression.bat -SkipBuild` | **PASS** (34/34) | Complete suite passed, 0 drift, 27 scenario framebuffers generated. |
| `python tools/generate_assets.py --check` | **PASS** | 25 canonical masters, 68 runtime icons verified. |
| `tools/export_mcu.bat --verify` | **PASS** | MCU export manifest verified. |
| `tools/verify_production.ps1` | **PASS** | All 4 phases passed (collision safety, isolated Release host build, smoke test, negative control). |
| `docs/reviews/2026-09-21-runtime-followup-delivery/probe.exe` | **PASS** | Detailed observational probe completed; log in `PROBE.txt`. |

### Uptime Measurements from Delivery Probe (`PROBE.txt`)

```text
UPTIME FOLLOWUP '100:00:00'    gap=23  label_x=518 width=97   clock_x=490 clock_dist=8
UPTIME FOLLOWUP '100:08:08'    gap=27  label_x=518 width=93   clock_x=490 clock_dist=8
UPTIME FOLLOWUP '188:58:58'    gap=35  label_x=518 width=85   clock_x=490 clock_dist=8
UPTIME FOLLOWUP '1000:00:00'   gap=22  label_x=506 width=110  clock_x=478 clock_dist=8
UPTIME FOLLOWUP '1888:58:58'   gap=36  label_x=506 width=96   clock_x=478 clock_dist=8
UPTIME FOLLOWUP '1193046:28:15' gap=47 label_x=467 width=124 clock_x=439 clock_dist=8
UPTIME RECONNECT '111:11:11'   gap=59  clock_dist=8
UPTIME AFTER RECONNECT '111:20:00' gap=37 clock_dist=8
```

All measured separator gaps are $\ge 22\text{px} \ge 14\text{px}$, and `clock_dist` is strictly 8px.

### Memory Impact Ledger

| Component | Before F4 | After F4 (Delivery) | Delta |
|---|---|---|---|
| Unused Style Descriptors (BSS) | 24 styles (384 B) | 0 styles (0 B) | **-384 B BSS** |
| Theme Styles Heap at Boot | 1,560 B | 0 B | **-1,560 B Heap** |
| Boot Total Free Heap | 6,896 B | 8,464 B | **+1,568 B Free** |
| Home Settled Free Heap (CHECK 31) | 5,992 B | 7,608 B | **+1,616 B Free** |
| Static Draw Buffer (`UI_DRAW_BUF_LINES=10`) | 16,000 B | 16,000 B | 0 B |
| Total Static UI BSS Reservation | 59,392 B | 59,008 B | **-384 B BSS** |

---

## Artifact Index

- **Delivery Review Report:** [`docs/reviews/2026-09-21-runtime-followup-delivery/REVIEW.md`](REVIEW.md)
- **Delivery Verification Log:** [`docs/reviews/2026-09-21-runtime-followup-delivery/VERIFICATION.md`](VERIFICATION.md)
- **Regression Suite Log:** [`docs/reviews/2026-09-21-runtime-followup-delivery/REGRESSION.txt`](REGRESSION.txt)
- **Independent Delivery Probe Log:** [`docs/reviews/2026-09-21-runtime-followup-delivery/PROBE.txt`](PROBE.txt)
- **Delivery Captures:** [`docs/reviews/2026-09-21-runtime-followup-delivery/captures/`](captures/)
