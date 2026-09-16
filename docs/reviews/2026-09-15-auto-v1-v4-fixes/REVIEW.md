# Verification Report — Auto V1–V4 Fixes & Acceptance Suite

**Date:** 2026-09-15  
**Scope:** Execution and verification of all scoped items V1–V4 from [CURRENT.md](file:///c:/Users/vanth/Documents/MyDocument/Embedded/LVGL_UI/docs/prompts/CURRENT.md).  
**Status:** Implemented, verified with isolated host build, 31/31 regression checks, and native 800x480 RGB565 screenshots.

---

## 1. Executive Summary

All findings V1–V4 from the technical lead review have been resolved in full:
1. **V1 [P2] Restore Integration Example Repair & Test:** Corrected `SMART_HUB_UI_GUIDE.md` section 8.4 and created a maintained host test suite in `sim_pc/test_restore_integration.c` compiling against real headers. Validated missing records, valid v2 records, legacy v1 migration with canonical defaults, truncated records, provenance checking, callback isolation (0 callbacks on restore), and outbound TX gating.
2. **V2 [P2] Open Dropdown List Typography, Bounding & Outside Dismiss:** Styled the open dropdown list in `ui.c` using `ui_font_18`, row pitch $\ge 44\text{px}$ (`line_space = 19`, pitch 44px), `border_width = 0`, and `max_height = 240px` explicitly bounding popup height within modal bounds $(180, 56)..(619, 423)$. Added outside dismiss scrim (`s_dialog_dd_scrim`) with pass-through click for the modal Close button (X) and automatic lifecycle cleanup in `ui_tick()` preventing memory leaks.
3. **V3 [P2] Maintained Acceptance Checks & Verification Suite:** Promoted acceptance checks 25–31 into the standard regression suite (`tools/run_regression.bat`). Corrected `home_auto_paused.raw` scenario to assert genuine pending-to-timeout progression, error latches, and dismissed toast. Added tests for invalid draft cancel, stepper validation, recording TX loop, Auto sensor hold/reconnect/re-arm lifecycle, and 50-cycle memory stress. Hardened `tools/verify_production.ps1` with bounded process watchdogs (30–60s), export artifact consumption, artifact membership verification, and strict negative control exit code 1 handling.
4. **V4 [P3] Balanced Home Tile Error/Offline Status Layout:** Rebalanced Home device tiles into two distinct metadata rows for composite error/offline states:
   - **Row 1:** Full-width single-line mode label at y=34 (`mode_w = 157`).
   - **Row 2:** State label ("Unknown" in red) at y=63 and actionable target ("Retry" in blue) at (103, 56) with extended click area (`ext_click_area = 6`, effective touch target $66 \times 44$).
   - Measured vertical line-box gap: $5\text{px} \ge 4\text{px}$.
   - Measured bottom clearance to tile edge: $16\text{px} \ge 10\text{px}$.
   - Preserved settled idle single-column layout at y=50/42 with 11–12px gap to switch.

---

## 2. Changed Files Mapping

| Item | Component / File | Key Modifications |
|---|---|---|
| **V1** | `sim_pc/test_restore_integration.c` | Compiled host test suite against real project headers; validated record kinds (missing, v2, v1 legacy, truncated), callback isolation, and outbound TX gating. |
| **V1** | `sim_pc/ui_test_api.h`, `ui_test_api.c` | Exposed `ui_test_set_boot_staging(bool)` hook to enforce zero persistence callbacks during flash restore tests. |
| **V1** | `docs/user/SMART_HUB_UI_GUIDE.md` | Updated section 8.4 snippet with exact 3-argument migration `ui_auto_migrate_legacy_config(&leg, sizeof(leg), &cand)` and provenance validation. |
| **V2** | `ui.c` | Styled open dropdown list with `ui_font_18`, `line_space = 19` (44px pitch), `border_width = 0`, `max_height = 240`. Added `s_dialog_dd_scrim` outside dismiss with X-button hit-testing and auto-cleanup. |
| **V3** | `sim_pc/main.c` | Promoted Checks 25–31 to regression suite; updated `home_auto_paused.raw` setup with strict error phase assertion and toast dismissal; added scenarios 22 & 23 (`dropdown_open_top`, `dropdown_open_bottom`). |
| **V3** | `tools/run_regression.ps1` | Extended timeout from 15s to 60s to accommodate 31 regression checks. |
| **V3** | `tools/verify_production.ps1` | Implemented `Invoke-ProcessWithTimeout` process watchdogs (30–60s), consumed exported staging bundle, verified artifact membership (16 files), and guarded cleanup. |
| **V4** | `ui.c` | Implemented two-row composite layout for Home tiles: Row 1 Mode at y=34, Row 2 State at y=63, Retry at (103, 56) with 66x44 touch target. |

---

## 3. Detailed Verification Results

### 3.1 V1: Flash Restore Integration Suite (`[CHECK 25]`)
- **Test file:** `sim_pc/test_restore_integration.c`
- **Scenarios tested:**
  1. Missing record (Ch 0) -> Defaults safely to Air Purifier, Manual mode.
  2. Valid Current V2 record (Ch 1) -> Restores Living Fan, Auto mode, limits 1200/800, active_low=1.
  3. Valid Legacy V1 record (Ch 2) -> Migrates Bed Humid, downgrades legacy Auto to Manual, canonical limits 40/50.
  4. Truncated Current V2 record (20 bytes) -> Rejected, falls back safely to Desk Light, Manual mode.
  5. Truncated Current V2 record with legacy length (sizeof v1) but kind=CURRENT_V2 -> Rejected by provenance check.
  6. Unknown record kind -> Rejected, falls back to channel default.
  7. Invalid threshold pair (ON 800 < OFF 1000 for Fan) in V2 record -> Rejected to canonical defaults.
  8. Persistence callbacks: Boot restore triggers exactly 0 callbacks; runtime commit triggers exactly 1; no-op save triggers 0; rejected edit triggers 0.
  9. Outbound TX gating: `ui_is_tx_ready()` verified false while link is disconnected/stale, and restores to true upon packet reconciliation.
- **Result:** **PASS**

### 3.2 V2: Open Dropdown Styling & Bounding (`[CHECK 26]`)
- **Measured geometry:**
  - Font: `ui_font_18` (18 px font height, line height 25 px).
  - Line space: `19 px` $\rightarrow$ Option pitch = $25 + 19 = 44\text{px}$ ($\ge 44\text{px}$ required).
  - Border width: `0 px` with light surface styling and subtle shadow.
  - Dropdown popup bounds: $(340, 164)..(599, 404)$ $\rightarrow$ Height 240 px, strictly within modal bounds $(180, 56)..(619, 423)$.
  - Outside dismiss scrim: Covers screen, dismisses dropdown on background click; clicking Close button (X) passes through to close modal immediately.
  - Lifecycle: `s_dialog_dd_scrim` auto-cleaned in `ui_tick()` and `close_device_dialog()`, leaving 0 dangling pointers.
- **Evidence:** `shots/dropdown_open_top.png` and `shots/dropdown_open_bottom.png`.
- **Result:** **PASS**

### 3.3 V4: Balanced Home Tile Error/Offline Layout (`[CHECK 28]`)
- **Measured geometry (Device 1 - Ventilation Fan):**
  - Tile outer bounds: $(205, 288)..(385, 403)$ (screen y 288..403, h=116, pad=12). Content origin $(217, 300)$.
  - Row 1 (Mode label): y=34 relative to tile content $\rightarrow$ screen y = 334..358 ($h=25\text{px}$). Width = 157 px.
  - Row 2 (State label): y=63 relative to tile content $\rightarrow$ screen y = 363..387 ($h=25\text{px}$).
  - Retry button: pos = $(103, 56)$, size = $54 \times 32$, ext click area = 6 px $\rightarrow$ effective hit area $66 \times 44\text{px}$ ($\ge 44 \times 44\text{px}$ required).
  - Vertical line-box gap: $363 - 358 = 5\text{px}$ ($\ge 4\text{px}$ required).
  - Bottom clearance: $403 - 387 = 16\text{px}$ ($\ge 10\text{px}$ required).
  - Pointer verification: Real pointer click on Retry ($(x=320..374, y=356..388)$) successfully cleared the `paused_error` latch.
- **Evidence:** `shots/home_auto_paused.png`, `shots/home_relay_retry.png`.
- **Result:** **PASS**

### 3.4 V3: Acceptance Suite & Production Verification
- **Regression Suite:** 31 / 31 checks passed in `tools/run_regression.bat`.
  - `[CHECK 27]` Invalid draft stepper creation (ON=1000, OFF=1000, gap 0 < 50), Save disabled, Cancel via X, discard on reopen, callback isolation.
  - `[CHECK 29]` Recording TX loop, sequence change immediate send, link staleness bound (6000ms > 5000ms), gated TX ordering.
  - `[CHECK 30]` Auto lifecycle: hold timer suppresses decisions during 10s post-transition; decision fires upon hold expiry and 3 qualified frames; timeout latches error; link disconnect and reconnect retains error latch; explicit Retry re-arms auto.
  - `[CHECK 31]` 50 complete cycles memory stress test.
- **Production Verification (`tools/verify_production.bat`):**
  - Phase 1: MCU export manifest verification PASS (`export_mcu.bat --verify`).
  - Phase 2: Isolated host build from exported artifacts (`export_dest\smart_hub_ui`) with `-Wall -Wextra -Werror` (watchdog 60s) PASS.
  - Phase 3: Clean production smoke run (exit code 0, watchdog 30s) PASS.
  - Phase 4: Negative control run (`--fail-check`, strictly exit code 1 + stderr matching, watchdog 30s) PASS.
  - Guarded cleanup: Task-owned temp directory removed on pass; retained on failure.
- **Result:** **PASS**

---

## 4. Performance & Memory Measurements

### 4.1 LVGL 42 KiB Heap Headroom (Measured in `[CHECK 8]` & `[CHECK 31]`)
| View / Checkpoint | Used % | Total Free (B) | Largest Block (B) | Fragmentation % |
|---|---|---|---|---|
| Splash screen | 73% | 11,992 | 9,600 | 20% |
| Home page (settled) | 82% | 7,864 | 6,000 | 24% |
| Trends page (data) | 60% | 17,344 | 9,600 | 45% |
| Trends page (empty) | 60% | 17,344 | 9,600 | 45% |
| Devices page | 73% | 11,672 | 9,600 | 18% |
| Dialog open | 89% | 4,888 | 3,928 | 20% |
| Dropdown open | 71% | 12,536 | 5,624 | 56% |
| **50-Cycle Stress Peak** | **82%** | **7,784** | **5,920** | **24%** |

*Note: All views and stressed states maintain $> 4,096\text{ B}$ free memory (minimum observed free: $4,888\text{ B}$ during dialog open, $7,784\text{ B}$ during sustained cycle stress). Zero progressive memory leaks detected across 50 cycles (stable drift: $+0\text{ bytes}$).*

### 4.2 Redraw & Flush Bounds
- Identical status frames (Home settled): 0 flushes, 0 pixels pushed.
- Idle Home 500 ms sub-second: 0 flushes, 0 pixels pushed.
- Idle Home 1000 ms full-second: 1 flush, 4,089 pixels pushed (uptime header string only).
- Per-tick change test: Revision counter comparison + link staleness check ($< 50\text{ ns}$, 0 flushes).
- Value round-trip idempotence: 0 differing pixels.
- Page rebuild idempotence: 0 differing pixels.

---

## 5. Screenshot Gallery (Native 800x480 RGB565)

All captures are stored under `docs/reviews/2026-09-15-auto-v1-v4-fixes/shots/`:
- **Corrected Home Error Status Spacing:** [home_auto_paused.png](file:///c:/Users/vanth/Documents/MyDocument/Embedded/LVGL_UI/docs/reviews/2026-09-15-auto-v1-v4-fixes/shots/home_auto_paused.png)
- **Open Dropdown (Top of List):** [dropdown_open_top.png](file:///c:/Users/vanth/Documents/MyDocument/Embedded/LVGL_UI/docs/reviews/2026-09-15-auto-v1-v4-fixes/shots/dropdown_open_top.png)
- **Open Dropdown (Scrolled to Bottom):** [dropdown_open_bottom.png](file:///c:/Users/vanth/Documents/MyDocument/Embedded/LVGL_UI/docs/reviews/2026-09-15-auto-v1-v4-fixes/shots/dropdown_open_bottom.png)
- **Invalid Threshold Draft (Save Disabled):** [devices_auto_invalid_draft.png](file:///c:/Users/vanth/Documents/MyDocument/Embedded/LVGL_UI/docs/reviews/2026-09-15-auto-v1-v4-fixes/shots/devices_auto_invalid_draft.png)
- **Home Relay Retry Action:** [home_relay_retry.png](file:///c:/Users/vanth/Documents/MyDocument/Embedded/LVGL_UI/docs/reviews/2026-09-15-auto-v1-v4-fixes/shots/home_relay_retry.png)
- **Home Offline State:** [home_offline.png](file:///c:/Users/vanth/Documents/MyDocument/Embedded/LVGL_UI/docs/reviews/2026-09-15-auto-v1-v4-fixes/shots/home_offline.png)
- **Home Sensor Failure:** [home_sensor_fail.png](file:///c:/Users/vanth/Documents/MyDocument/Embedded/LVGL_UI/docs/reviews/2026-09-15-auto-v1-v4-fixes/shots/home_sensor_fail.png)
- **Home Temp/Humid Invalid (Preserved Em-Dash):** [home_temp_humid_invalid.png](file:///c:/Users/vanth/Documents/MyDocument/Embedded/LVGL_UI/docs/reviews/2026-09-15-auto-v1-v4-fixes/shots/home_temp_humid_invalid.png)
- **Devices Settings (Device View):** [devices_settings.png](file:///c:/Users/vanth/Documents/MyDocument/Embedded/LVGL_UI/docs/reviews/2026-09-15-auto-v1-v4-fixes/shots/devices_settings.png)
- **Devices Settings (Auto Threshold View):** [devices_auto_thresholds.png](file:///c:/Users/vanth/Documents/MyDocument/Embedded/LVGL_UI/docs/reviews/2026-09-15-auto-v1-v4-fixes/shots/devices_auto_thresholds.png)

---

## 6. Open Target Hardware Limitations (Marked OPEN)

Per engineering guidelines, the following hardware aspects are outside host simulation acceptance and remain **OPEN** until target bench testing:
1. **Target MCU QSPI Display Driver:** DMA transfer rates and TE sync timing under STM32/ESP32 QSPI controllers.
2. **Capacitive Touch Latency:** Physical I2C/SPI touch controller interrupt response and debouncing on physical glass.
3. **920 MHz LoRa RF Propagation:** Physical packet loss rates, antenna impedance matching, and RSSI/SNR calibration in real RF environments.
4. **Flash/NVS Persistence Driver:** Physical wear-leveling, flash sector erase cycles, and power-cut safety on physical SPI Flash.
5. **RS-485 Modbus Sensor Bus:** Physical transceiver turnaround timing, bus termination reflections, and sensor response timeouts on target hardware.
