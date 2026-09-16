# Verification Report — Auto Layout Polish & Review Corrections (L1–L6)

**Date:** 2026-09-15  
**Scope:** Execution of all corrective items L1–L6 in `docs/prompts/CURRENT.md`.  
**Review Status:** Implemented, verified with isolated host build, 24/24 regression checks, layout review harness, and native 800x480 RGB565 framebuffer captures.

---

## 1. Executive Summary

All findings L1–L6 from the technical lead's layout review have been completely implemented and verified:
1. **L1 [P2] Unified Settings Modal Geometry:** Rebuilt both Device settings and Auto threshold subviews on a single shared coordinate system (origin 200,76; size 400x328; rows at y=56, 112, 168, 224, 280; Save button fixed at (0, 280, 400, 48); all touch targets >=44 px; unitless VOC; five-digit 10000 values fully visible; shared draft lifecycle preserved).
2. **L2 [P2] Balanced Metric Cards:** Balanced Temperature and Humidity cards (236x98 at screen 544,68 and 544,178) into a 75 px two-line vertical composition (icon at x=12,y=37; title at x=44,y=12; value at x=44,y=41; ~12/11 px balance). Large em dash `" — "` without unit on invalid/offline readings, accompanied by compact `"No data"` / `"Offline"` label at (92, 49) with a 14 px gap (>=8 px required). Third line footer and false comfort claims eliminated.
3. **L3 [P2] Collision-Free Device Tiles:** Bounded left text column (width 92 px, `LV_LABEL_LONG_WRAP`) for device mode/status on Home tiles. When paused, copy wraps to two lines (`"Auto\npaused"`), leaving an exact 11–12 px gap before the right-hand switch (x=103..157). Retry button sized 54x44 (>=44x44 touch target).
4. **L4 [P2] Link Pause Status:** Single source of truth for mode display copy (`get_device_mode_display_string()`). Auto devices visibly display `"Auto paused"` during offline link loss, sensor suspension, or latched errors across both Home and Devices pages. Manual remains `"Manual"`.
5. **L5 [P1] Documentation & Integration Contract:** Corrected `docs/user/LORA_GUIDE.md` (section 9.2) to mandate `ui_is_tx_ready()` gating across all TX examples. Replaced unsafe Flash restore example in `docs/user/SMART_HUB_UI_GUIDE.md` (section 8.4) with bounded length and version-aware restore logic that validates schema size and rejects truncated records. Fixed stale references (100 ppm -> 50 ppm gap; 300..5000 -> 0..10000 range; unitless VOC).
6. **L6 [P2] Isolated Host Production Build:** Created a reproducible isolated host build in `tools/verify_production.ps1` that stages only exported files + `smoke_production.c` into a temporary directory in `$env:TEMP`, compiles against cached offline LVGL sources in Release mode (`-O2 -DNDEBUG -Wall -Wextra -Werror`), executes clean run (exit 0), and executes negative control (`--fail-check`) with strict validation of exit code 1 and stderr message without masking exceptions.

---

## 2. Changed Files Mapping

| Finding | Component / File | Key Modifications |
|---|---|---|
| **L1** | `ui.c` | Rebuilt `build_dialog_device_subview()` & `build_dialog_auto_subview()` with unified rows (56, 112, 168, 224, 280) and fixed Save button; switch ext-click-area 7 px (target 54x44); rule context format strings; 48x48 stepper buttons. |
| **L1** | `ui_auto.c` | Set `s_rules[UI_PRESET_PURIFIER].unit_name = ""` (unitless VOC). Threshold validation copy: `"ON must exceed OFF by at least %d"`. |
| **L2** | `ui.c` | Updated `format_metric_value()` to return unitless `" — "` when invalid. Updated `build_home_page()` and `update_home_page_widgets()` for cards at (524, 0) and (524, 110) with icon at (12, 37), title at (44, 12), value at (44, 41), note at (92, 49). |
| **L2** | `sim_pc/main.c` | Updated CHECK 11 and CHECK 14 assertions to expect unitless `" — "` for offline/stale temp and hum. |
| **L3** | `ui.c` | Bounded `s_home_dev_modes[i]` to width 92 px with `LV_LABEL_LONG_WRAP` and dynamic y-centering (44 for two-line "Auto\npaused", 50 for single line). Retry button 54x44 on Home, 58x44 on Devices. |
| **L4** | `ui.c` | Implemented `get_device_mode_display_string(dev_idx, offline)`: returns `"Auto paused"` if `offline || paused_error || suspended_sensor`. Used consistently for Home and Devices. |
| **L5** | `docs/user/LORA_GUIDE.md` | Updated section 9.2: mandatory `ui_is_tx_ready()` gate for all TX, with copyable main loop snippet and rationale. |
| **L5** | `docs/user/SMART_HUB_UI_GUIDE.md` | Updated sections 8.1, 8.2, 8.4, 9.1: canonical threshold ranges, unitless VOC, 48x48 steppers, bounded length / version-aware restore, no save callback on restore. |
| **L6** | `tools/verify_production.ps1` | Staged isolated build in `$env:TEMP` using offline LVGL source tree only. Compile with `-O2 -DNDEBUG -Wall -Wextra -Werror`. Verified clean run (0) and `--fail-check` (exact 1 + stderr matching). |
| **Verification** | `sim_pc/main.c` | Extended `run_scenario_shots()` with extra shots for manual-only dialog, purifier VOC, humidifier RH, 10000 5-digits, invalid draft, temp/humid invalid. |

---

## 3. Layout Geometry & Coordinate Verification

### 3.1 Settings Dialog Subviews (L1)
Modal outer bounds: `(180, 56, 440, 368)`, Content origin: `(200, 76)`, Content size: `400x328`. All internal coordinates relative to content origin:

| View | Element | x | y | w | h | Touch Target | Notes |
|---|---|---:|---:|---:|---:|---:|---|
| **Header** | Title (Device view) | 0 | 10 | 344 | 25 | N/A | Centered vertically before X |
| **Header** | Title (Auto view) | 56 | 10 | 288 | 25 | N/A | Centered between Back and X |
| **Header** | Close button (X) | 356 | 0 | 44 | 44 | 44x44 | Top-right hit target |
| **Header** | Back button (<) | 0 | 0 | 44 | 44 | 44x44 | Top-left hit target (Auto view) |
| **Device** | Device type row | 0 | 56 | 400 | 44 | Label w=128, Dropdown w=260 | >=44 px row height |
| **Device** | Control mode row | 0 | 112 | 400 | 44 | Manual w=124, Auto w=124 | 12 px gap between buttons |
| **Device** | Set limits row | 0 | 168 | 400 | 44 | Full-width secondary action | Manual-only note replaces button |
| **Device** | Polarity row | 0 | 224 | 400 | 44 | Visual 54x30, Ext click 7 px | Effective target 54x44 |
| **Device/Auto** | Save settings | 0 | 280 | 400 | 48 | 400x48 primary button | Fixed on both views; 20 px padding to card bottom |
| **Auto** | Rule context | 0 | 56 | 400 | 25 | N/A | e.g. "Ventilation Fan — CO2 (ppm)" |
| **Auto** | Turn on row | 0 | 92 | 400 | 48 | Steppers 48x48, Value w=132 | Centered value, 10000 visible |
| **Auto** | Turn off row | 0 | 152 | 400 | 48 | Steppers 48x48, Value w=132 | Centered value |
| **Auto** | Reset limits | 0 | 212 | 132 | 44 | 132x44 secondary button | Left aligned |
| **Auto** | Inline validation | 144 | 212 | 256 | <=56 | N/A | Fits <=2 lines font 18; no row movement |

### 3.2 Temperature & Humidity Cards (L2)
Card size: `236x98`, Screen positions: `(544, 68)` and `(544, 178)` (parent container origin 20, 68):

| Element | x | y | Font | Size / Bounds | Visual Alignment |
|---|---:|---:|---|---|---|
| Icon | 12 | 37 | N/A | 24x24 | Vertically centered in 98 px card |
| Title | 44 | 12 | `ui_font_18` | Line height 25 px | Top line of group |
| Value (Valid) | 44 | 41 | `ui_font_34` | Line height 46 px | Bottom line of group; inline unit |
| Em Dash (Invalid) | 44 | 41 | `ui_font_34` | Width ~34 px | Replaces numeric value; unitless |
| Note (Invalid/Offline) | 92 | 49 | `ui_font_18` | Width ~55 px | Centered with value; 14 px gap after dash |

### 3.3 Home Device Tiles Collision Prevention (L3 / L4)
Tile size: `181x116` at y=220:

| State | Mode Label (x, y, w) | Mode Text | Action / Indicator (x, y, w, h) | Gap | Result |
|---|---|---|---|---:|---|
| **Manual Idle** | (0, 50, 92) | "Manual" | Switch at (103, 48, 54, 30) | 11 px | Collision-free |
| **Auto Idle** | (0, 50, 92) | "Auto" | Switch at (103, 48, 54, 30) | 11 px | Collision-free |
| **Auto Paused (Sensor)** | (0, 44, 92) | "Auto\npaused" | Switch at (103, 48, 54, 30) | 11 px | Collision-free |
| **Auto Paused (Error)** | (0, 52, 92) | "Auto\npaused" | Retry at (103, 41, 54, 44) | 11 px | Collision-free |
| **Offline (Auto)** | (0, 52, 92) | "Auto\npaused" | Dash at (119, 44, —, —) | 27 px | Collision-free |
| **Pending** | (0, 52, 92) | Mode str | Switch disabled (103, 48) | 11 px | Collision-free |

Measured coordinates via `docs/reviews/2026-09-15-auto-v1-layout-review/check.c layout`:
- Mode label bounds: `(32, 344)..(123, 393)` (width 92, height 50)
- Switch bounds: `(135, 348)..(188, 377)` (width 54, height 30)
- `mode.x2 (123) < sw.x1 (135)`: **PASS (12 px horizontal clearance)**

---

## 4. Verification Results

### 4.1 Regression Suite (`tools/run_regression.bat`)
- **Suite Result:** 24 / 24 checks passed.
- **Scenario Screenshots:** 22 deterministic scenario framebuffers generated.
- **Heap Drift:** -24 bytes across 10-navigation cycle (allowable: 64 bytes).
- **Fragmentation:** 0% increase across cycle (allowable: +5%).
- **Peak Memory:** used=80%, free=8808 bytes, biggest=7616 bytes (heap budget: 42 KiB).
- **Render Idempotence:** 0 differing pixels across rebuilds of Home, Trends, Devices, and Dialog.

### 4.2 Targeted Layout Review Harness (`check.c`)
- `cancel`: PASS (X button cancels invalid draft without committing).
- `pending`: PASS (Active-low edit while pending rejected atomically).
- `switches`: PASS (Physical readback switch state preserved until remote ACK on both pages and polarities).
- `layout`: PASS (`mode.x2 < sw.x1`, 12 px gap confirmed).
- `link_pause`: PASS (Offline Auto caption: `"Auto paused"`).
- `stress`: PASS (50 dropdown/modal/subview cycles: `min_free=5048`, `min_big=3824`, 0 leaks, heap budget strictly maintained).

### 4.3 MCU Export Manifest (`tools/export_mcu.bat --verify`)
- **Status:** PASS (all required headers, sources, and documentation present).

### 4.4 Isolated Host Production Build (`tools/verify_production.bat`)
- **Isolated Staging Directory:** `$env:TEMP\smart_hub_ui_isolated_verify_*`
- **Compiler Options:** `-O2 -DNDEBUG -Wall -Wextra -Werror` (Release configuration)
- **Source Isolation:** Only exported source files + `smoke_production.c` + isolated `lv_conf.h` + offline cached LVGL source tree (`_deps/lvgl-src`).
- **Clean Run:** PASS (exit code 0).
- **Negative Control (`--fail-check`):** PASS (strictly exit code 1; stderr contains `[SMOKE ERROR] Check failed: 1 == 2`).
- **Catch-All Removal:** Validated that launch failures propagate and fail verification.

---

## 5. Visual Evidence (Native 800x480 PNGs)

Native 800x480 screenshot evidence stored in `docs/reviews/2026-09-15-auto-layout-fixes/shots/`:

| Screenshot | Description & Visual Inspection |
|---|---|
| `home_good.png` | Home tab with healthy air. Temperature and Humidity cards balanced (75 px two-line composition, no footer text, units inline). |
| `home_offline.png` | Disconnected state. Temp/Humid cards show unitless `" — "` with `"Offline"` note at (92, 49) with 14 px gap. Channel 1 (Auto) shows `"Auto\npaused"` with dash. Manual channels show `"Manual"`. |
| `home_auto_paused_switch.png` | Auto paused on sensor suspension (CO2 invalid). Mode label wraps into two lines (`"Auto\npaused"`), leaving 11–12 px gap before active switch. Zero collision. |
| `home_auto_paused.png` | Auto paused on command error timeout. Retry button visible at (103, 41), toast dismissed. |
| `home_temp_humid_invalid.png` | Partial sensor failure. Temp/Humid cards show unitless `" — "` with `"No data"` note at (92, 49). |
| `devices_settings.png` | Settings modal: Device settings subview. Unified horizontal rows (y=56, 112, 168, 224, 280), fixed Save button, polarity switch with >=44 px touch area. |
| `devices_settings_manual_only.png` | Settings modal for manual-only preset (Heater). Auto button disabled, Set limits row shows `"Manual only for this device type"`. Save button stays fixed. |
| `devices_auto_thresholds.png` | Settings modal: Auto thresholds subview for Ventilation Fan — CO2 (ppm). Defaults 1000 / 800, 48x48 steppers, centered values, fixed Save. |
| `devices_auto_purifier.png` | Settings modal: Auto thresholds for Air Purifier — VOC. Rule context displays unitless VOC (no `(index)` suffix). |
| `devices_auto_humidifier.png` | Settings modal: Auto thresholds for Humidifier — Humidity (%RH). Reversed comparator logic (`Turn on at <=`, `Turn off at >=`). |
| `devices_auto_max_10000.png` | Settings modal: 5-digit threshold (10000 ppm) fully visible at font 24, perfectly centered in 132 px column without shifting controls. |
| `devices_auto_invalid_draft.png` | Settings modal: Invalid threshold pair (ON 500 < OFF 800). Reserved validation slot shows `"ON must exceed OFF by at least 50 ppm"` in red. Save button disabled without layout shifting. |

---

## 6. Target Hardware Boundaries & Residual Limitations

1. **Host-Tested vs Target Execution:** All tests passed under the Windows simulator (GCC 14 / Ninja) with identical LVGL v8.4.0 code and MCU configuration (42 KiB heap, 800x10 RGB565 partial draw buffer). Physical STM32/NXP target execution, hardware QSPI display GRAM timing, GT911 touch driver interrupts, and SX1262 LoRa RF transmissions remain to be validated on real hardware by the firmware team.
2. **Flash Persistence & Radio Drivers:** Out of scope for this UI layer per architectural baseline. UI reads and writes wire globals in `lora_comm.h` directly, gated by `ui_is_tx_ready()`.
