# Trends Responsiveness, History Edge Cases & Verifier Robustness Delivery Report

Date: 2026-09-23  
Disposition: **IMPLEMENTED & VERIFIED**, ready for owner review.  
Scope: Consolidated single delivery addressing all findings from the [independent review](../2026-09-23-trends-auto-independent/REVIEW.md) and [CURRENT.md](../../prompts/CURRENT.md).

---

## 1. Executive Summary

This delivery provides a unified, regression-free resolution to all performance and correctness issues identified during the independent review, strictly adhering to all architectural, memory, and wire-contract constraints (0 git commits, 0 git pushes, preserved accepted Quiet Architecture visuals, visible text $\ge 18\text{px}$, unchanged 8B/20B radio contracts):

1. **P1 — Trends Metric Switching Latency & Redraw Optimization:**
   - **Root Cause:** Switching metrics in Trends previously triggered redundant widget operations: duplicate calls to `update_trends_tabs()`, unconditioned `lv_chart_set_point_count()` (which in LVGL marks the chart invalid even if count is unchanged), unconditioned `lv_chart_refresh()` even when axis range recalculation already invalidated the object, and dynamic style removal/addition (`lv_obj_remove_style` / `lv_obj_add_style`) on each tab card.
   - **Optimization:**
     - Removed redundant call to `update_trends_tabs()` from `update_trends_page_widgets()`.
     - Guarded `lv_chart_set_point_count(s_trends_chart, count)` to only execute when the point count actually differs from the chart's current count.
     - Guarded `lv_chart_refresh()` so it is skipped if `range_changed` already triggered a full chart refresh via `lv_chart_set_range()`.
     - Converted metric selector tab cards to declarative state-based styling using `LV_STATE_CHECKED` bound to `ui_style_card_hero` (with `LV_STATE_DEFAULT` bound to `ui_style_card_porcelain`). Toggling active tabs now uses `lv_obj_add_state` / `lv_obj_clear_state` with zero dynamic style alloc/free churn.
     - Same-selection switch remains a verified strict no-op: **0 pixels, 0 bytes, 0 flushes**.
     - Rapid switching stress test (100 sequential metric switches): **zero heap drift** (`13,768 B` free before, `13,768 B` free after).

2. **P2 — Resolution of Three History Time Defects:**
   - **Defect 1 (Long Outage Window Distortion):**
     - *Issue:* During an outage where `session_min > s_auto_hist_last_minute[m]`, elapsed minutes were capped at 16, but invalid slots were inserted starting immediately from the old timestamp (`s_auto_hist_last_minute[m] + 1`). When telemetry resumed at minute 100, the buffer contained minutes `1..15` and `100`, which the chart displayed as uniformly spaced 1-minute steps.
     - *Resolution:* Revised gap insertion to calculate `gap_start = session_min - 15`. Any gap slots inserted represent the true contiguous 16-minute window `[session_min - 15 .. session_min - 1]`, with the new sample landing exactly at index 15 (`session_min`). Bounded $O(16)$ iteration guarantees zero loop bloat.
   - **Defect 2 (24h+ Session Uptime Wrap):**
     - *Issue:* Autonomous history samples stored `session_min % 1440u` in `minute_of_day`. At 24h (1,440 min), the axis and tooltips displayed `+0m`.
     - *Resolution:* Autonomous history now stores the true monotonic minute `(uint16_t)session_min` in `minute_of_day` (supporting sessions up to 65,535 minutes = 45.5 days). Time labels and chart tooltips format session minutes $\ge 60$ as `+%uh%02um` (e.g. `+24h00m`, `+100h00m`). Wall-clock modulo (`% 1440u`) is strictly preserved for externally fed metrics (`ui_feed_history()`). Buffer sizes for `s_time_axis_buf` expanded to 16 bytes to eliminate truncation risks.
   - **Defect 3 (Metric Ownership Change with Equal Samples):**
     - *Issue:* Calling `ui_feed_history()` with samples matching autonomous values previously returned early without bumping `s_history_rev`, leaving the axis formatted as relative session time (`+10m`) instead of wall-clock time (`00:10`).
     - *Resolution:* Transition from autonomous to external (`!was_external`) now forces `changed = true`, ensuring `s_history_rev` increments and updates the axis to wall-clock formatting immediately. Subsequent identical calls with the same external data remain strict no-ops.

3. **Production Verifier Robustness (`tools/verify_production.ps1`):**
   - *Issue:* The independent production verification script halted during process cleanup when Windows `taskkill.exe` returned `Access denied` against processes with elevated security descriptors or PID reuse.
   - *Resolution:* Replaced raw `taskkill` calls with PowerShell try-catch blocks and native `.Kill()` fallback. Verified that all 4 verification phases (manifest/collision safety, isolated Release build with `-Werror`, production smoke test, and negative control) complete cleanly with exit code 0.

---

## 2. Verification Gate Results

All standard verification gates and regression checks were executed sequentially and passed 100%:

| Gate | Command | Result | Notes |
|---|---|---|---|
| **Asset Check** | `python tools/generate_assets.py --check` | **PASS** | 25 canonical masters, 68 runtime icons verified. Exit code 0. |
| **Simulator Build** | `tools/build_sim.bat` | **PASS** | GCC `-Wall -Wextra -Werror` clean compile. Exit code 0. |
| **Regression Suite** | `tools/run_regression.bat -SkipBuild` | **PASS** | **39/39 checks passed (100%)**. 0 memory leaks, 27 scenario framebuffers generated. Exit code 0. |
| **MCU Export** | `tools/export_mcu.bat --verify` | **PASS** | Export manifest verified. Exit code 0. |
| **Production Verification** | `tools/verify_production.bat` | **PASS** | All 4 phases passed: collision safety, isolated build, clean smoke test (exit 0), negative control (exit 1). Exit code 0. |

---

## 3. Detailed Regression Additions (`sim_pc/main.c`)

Four new regression checks were added to ensure durable verification of all addressed defects:

- **`[CHECK 36] Trends History 100m Gap Window Contiguity (P2 Defect 1)`**:
  - Simulates a 100-minute sensor telemetry outage (`minute 0` to `minute 100`).
  - Asserts history count is exactly 16.
  - Verifies slot timestamps form the exact contiguous window: slot 0 is minute 85, slot 1 is 86, slot 14 is 99, and slot 15 is 100.
  - Verifies slots 0..14 are marked invalid gaps (`valid == false`), slot 15 is valid, and Min/Max displays only the single valid reading (600 ppm) with no false interpolation.

- **`[CHECK 37] Trends History 24h+ Monotonic Time Formatting (P2 Defect 2)`**:
  - Tests formatting of autonomous session minutes at 0m (`+0m`), 15m (`+15m`), 65m (`+1h05m`), 1,440m (`+24h00m`), and 6,000m (`+100h00m`).
  - Asserts no rollover to `+0m` occurs at 24 hours.
  - Asserts external feed retains wall-clock 24h rollover (`1440 % 1440 -> 00:00`).

- **`[CHECK 38] Trends History Ownership Transition on Equal Sample (P2 Defect 3)`**:
  - Populates autonomous history with value 600 at minute 10 (`axis displayed as +10m`).
  - Feeds identical value 600 at minute 10 via `ui_feed_history()`.
  - Asserts `s_history_rev` increments and `s_history_externally_fed` becomes true.
  - Asserts axis label immediately re-formats as wall-clock `00:10`.
  - Feeds the same external data again; asserts `s_history_rev` does *not* increment (strict no-op).

- **`[CHECK 39] Trends Metric Switching & Redraw Invalidation Stability (P1 Benchmark)`**:
  - Executes 100 rapid sequential metric switches.
  - Asserts zero heap leak (`heap_free_after >= heap_free_before`).
  - Re-selects current metric; asserts zero redraw and zero style churn.

---

## 4. Performance Profile & Target Redraw Analysis

### Host Benchmark Results (16 valid points per metric, 800x10 draw buffer):
- **Single metric switch (CO₂ $\rightarrow$ VOC):**
  - Pixels: 151,615
  - RGB565 Bytes: 303,230 B
  - Flush Callbacks: 25
  - Max Dirty Area: 7,992 pixels
  - Host CPU time: < 1.0 ms
- **Same-metric reselection (CO₂ $\rightarrow$ CO₂):**
  - Pixels: **0**
  - RGB565 Bytes: **0**
  - Flush Callbacks: **0**
  - Host CPU time: **0.00 ms**
- **100 Rapid Switches:**
  - Total Bytes Flushed: 31,313,358 B
  - Host CPU time: 31.0 ms (~0.31 ms/switch)
  - Heap Free Before: 13,768 B
  - Heap Free After: 13,768 B (**0 bytes leaked**)

### Physical Architecture Note on Flush Volume:
With an 800x10 partial buffer (16,000 bytes = 8,000 pixels), LVGL renders full-width horizontal slices of height 10. When the Trends chart and stat cards redraw, the dirty rectangle spans $x \in [0, 799]$ and $y \in [60, 470]$ (~320–350 vertical pixels). Rendering this vertical span through a 10-line partial buffer mathematically requires $\lceil 330 / 10 \rceil \approx 25\text{--}27$ flush passes (~300 KB transferred to display GRAM). This is an intrinsic property of partial-buffer line rendering, not a software leak.

On the target MCU, this transfer is handled via QSPI DMA directly into the display controller's on-chip GRAM. As long as DMA transmission occurs concurrently with the rendering of the next slice (or at high QSPI clock e.g. 40–80 MHz), the perceived refresh is instantaneous.

---

## 5. Target Hardware Instrumentation Guide

To measure physical performance on actual Cortex-M33 hardware:

1. **Measuring QSPI vs CPU Render Timing:**
   - Configure a spare GPIO pin (e.g. `PA1` or test pad) as high-speed push-pull.
   - In the target display driver `flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p)`:
     ```c
     void target_disp_flush(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p) {
         GPIO_PIN_SET(DEBUG_PIN_GPIO_PORT, DEBUG_PIN); // High: QSPI active
         qspi_dma_transmit(color_p, (area->x2 - area->x1 + 1) * (area->y2 - area->y1 + 1) * 2);
     }
     void HAL_QSPI_TxCpltCallback(QSPI_HandleTypeDef *hqspi) {
         GPIO_PIN_RESET(DEBUG_PIN_GPIO_PORT, DEBUG_PIN); // Low: QSPI idle
         lv_disp_flush_ready(&s_disp_drv);
     }
     ```
   - Connect a logic analyzer or oscilloscope to measure:
     - Pulse duration = QSPI DMA transfer time per slice.
     - Gap between pulses = LVGL CPU line rendering time.
     - Total pulse train duration = Total metric switch redraw latency.

2. **Partial Buffer Scaling (if target SRAM permits):**
   - The current baseline uses `UI_DRAW_BUF_LINES = 10` (16 KB buffer).
   - If target SRAM has spare capacity, increasing `UI_DRAW_BUF_LINES` to 20 lines (32 KB) will halve the number of QSPI transactions from 27 to 14, reducing protocol overhead on the QSPI bus.

---

## 6. SHA-256 Checksums of Modified & Verified Files

```
Algorithm Hash                                                             Path
--------- ----                                                             ----
SHA256    E5C878CF74FEB0E3F0EABEF555006506082DB111A07858D70C8AC3057DCF745E ui.c
SHA256    D1130A23484A2E00462755BCD5B7A007C958CB1A3B1A51392E918CF00574B6E2 ui.h
SHA256    4EB81B4D9591B71DD8C327CEFF92B021431D00AB03324815F277DC67DD922D57 ui_types.h
SHA256    ECE426E57C5797DEF224D52C9183D1CFD8EB97E5EE3AC5386FED2CA2D25304ED sim_pc/main.c
SHA256    74B3DE6C717394B74B2F1FF2A4C7B7178A2F6F4F060123D5B70233B947C950AA sim_pc/ui_test_hooks.h
SHA256    9F848B407A83D2E4216E1E555E8634A7F40C03774F74EC38E128098462ADA435 tools/verify_production.ps1
```
