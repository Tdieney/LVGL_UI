# Consolidated Delivery Report — LoRa Protocol v2 and Trends Refinements

**Date:** 2026-09-25  
**Scope:** One consolidated delivery implementing LoRa protocol v2 simulator/test suite updates (Part A) and owner-decided Trends refinements B1–B4 (Part B) as specified in [docs/prompts/CURRENT.md](../../prompts/CURRENT.md).  
**Status:** IMPLEMENTED — all verification gates passed; ready for final technical-lead review.  
**Branch / Worktree:** `ui/smart-hub` (pre-existing worktree strictly preserved; 0 commits, 0 pushes).

---

## 1. Executive Summary

This consolidated delivery implements both Part A (LoRa protocol v2 simulator/test updates) and Part B (Trends axis/selector refinements) without altering accepted Quiet Architecture screens, keeping all visible text $\ge 18\text{ px}$, and maintaining full embedded memory constraints.

### Part A: LoRa Protocol v2 Simulator & Test Updates
1. **Telemetric Type Renaming (`tvoc_ppb`):**
   - Replaced all legacy `voc_index` references across `sim_pc/` (`main.c`, `ui_test_api.c`, `test_restore_integration.c`, `ui_test_hooks.h`) with `tvoc_ppb` representing raw parts-per-billion values ($0\dots 60000\text{ ppb}$).
   - Updated UI labels and card titles to `eCO₂` / `TVOC`.
2. **LoRa Request/Response Polling & State Synchronization:**
   - Updated fake Node emulation in `sim_pc/main.c` and `sim_pc/ui_test_api.c` according to `LORA_PROTOCOL.md` §3.3:
     - Smart Hub polls via `GET_STATUS` (0x03) approximately once per second.
     - Hub issues `HUB_CMD` (0x01) only when `ui_is_tx_ready() && ui_has_pending_command()`, capped at 3 attempts per sequence number.
     - Fake Node replies to both frame types and only modifies relay states upon valid `HUB_CMD`.
     - Fake Node increments `sample_seq` on every completed sensor acquisition.
3. **Qualification & Sequence Number Gating:**
   - Auto evaluation strictly gates qualification on new `sample_seq` values.
   - Updated test helpers in `sim_pc/` to advance `sample_seq` when simulating fresh readings, fixing `home_auto_paused` transition checks.
4. **Durable v2 Regression Coverage (Check 41):**
   - Added durable regression assertions validating:
     - `sample_seq` gating: repeated reports with the same sequence number do not advance Auto qualification; 3 distinct samples do.
     - `ui_has_pending_command()` assertion during relay toggle / setpoint edits and clearing upon link ACK.
     - Node reboot re-sync and command timeout write-back behavior.

### Part B: Owner-Decided Trends Refinements
1. **B1 (Restored Selector Card Highlight & Indicator):**
   - Left-rail metric cards apply `ui_style_card_hero` when checked (`LV_STATE_CHECKED`), turning the selected card background to deep green (`UI_COLOR_FOREST`), title/icon/value/unit to white (`UI_COLOR_WHITE`), and displaying a celadon $\varnothing 8\text{ px}$ indicator dot at $(160, 14)$.
   - Removed legacy 6x48 selection bar.
2. **B2 (Layout Geometry Adjustments):**
   - Title lowered to $(20, 14)$ with `Data unavailable` badge at $(-20, 23)$.
   - Y-axis unit label raised to $(12, 96)$.
   - 3 Y tick labels positioned at $y=136$, $191$, and $246$.
   - Chart object repositioned to $(72, 136)$, size $328 \times 128\text{ px}$.
   - Time tick labels positioned at $y=276$.
   - Empty state box positioned at $(20, 108)$.
3. **B3 (15-Minute Fixed Window & Empty States):**
   - Fixed 15-minute X-axis window across 16 points with newest point at slot 15.
   - Autonomous history labels: `"-15 min"`, `"-10 min"`, `"-5 min"`, `"Now"`.
   - Wall-clock history labels: `HH:MM` without duplicate label bugs.
   - 0-sample empty state: `"No readings yet"`.
   - 1-sample collecting state: `"Collecting readings…"` with explanatory subtitle.
4. **B4 (Round-Number Y Ticks & 64-bit Overflow Safety):**
   - 3 round-number Y-axis labels ($lo$, $lo+s$, $lo+2s$) with step $s \in \{1, 2, 2.5, 5\} \times 10^k$.
   - Negative temperature support using mathematical floor rounding.
   - Widen Y-axis arithmetic to `int64_t` and clamp to `[INT32_MIN, INT32_MAX]` preventing signed overflow on extreme external feeds (`INT32_MAX`/`INT32_MIN`), verified via fail-closed regression tests in CHECK 10.

### Technical-Lead Review Follow-up (F1–F3)
- **F1 (Signed Overflow Resolution & Extreme Bounds Regression):** Widened `trends_calc_y_axis()`, `format_temp_c()`, and tick math to 64-bit with `clamp_to_i32()` clamping. Added fail-closed regression checks feeding $2147483000\dots 2147483647$ and $-2147483648\dots -2147483000$, asserting strictly increasing bottom→top labels without wrap.
- **F2 (Documentation Synchronization):** Updated stale "simulator does not build" statements across `docs/UI_DESIGN_BRIEF.md`, `docs/user/SMART_HUB_UI_GUIDE.md`, and `docs/MCU_BASELINE.md` to reflect the passing simulator and test suites (41/41).
- **F3 (Attribution Accuracy):** Reworded `docs/UI_DESIGN_BRIEF.md` to state that the owner decided the five design directions, with pixel coordinates and tick algorithms proposed by the technical lead.

---

## 2. Verification Gates Summary

All verification gates were executed synchronously post-edit on the Windows host environment:

| Gate | Exact Command Line | Exit Code | Compiler / Tool Status | Result / Check Count |
| :--- | :--- | :---: | :---: | :--- |
| **1. Asset Check** | `python tools/generate_assets.py --check` | `0` | Clean | 25 canonical masters, 68 runtime icons verified |
| **2. Simulator Build** | `tools/build_sim.bat` | `0` | **0 errors, 0 warnings** | C build clean (`-Wall -Wextra -Werror`); 1 CMake CMP0177 dev warning noted |
| **3. Default 42 KiB Regression** | `tools/run_regression.bat` | `0` | Clean | **41 / 41 checks passed (100%)**; negative `--shots NUL` exit 1 verified; 30/30 manifest verified |
| **4. Opt-in 40 KiB Regression** | `cmake --build build-40k --target sim_pc`<br>`build-40k\sim_pc.exe --regression` | `0`<br>`0` | Clean | **41 / 41 checks passed (100%)**; min pool free 5,984 B > 4,096 B safety margin; 0 B drift |
| **5. MCU Export Check** | `tools/export_mcu.bat --verify` | `0` | Clean | 16 MCU bundle files verified against manifest |
| **6. Production Verifier** | `tools/verify_production.bat` | `0` | Clean | All 4 phases passed (Manifest, Isolated Build, Clean Smoke, Negative Control `--fail-check`) |
| **7. Whitespace Check** | `git diff --check` | `0` | Clean | 0 whitespace or formatting errors |

> **Upstream CMake Warning Note:**  
> The C compiler compiles with `-Wall -Wextra -Werror` producing **0 errors and 0 warnings**. Upstream CMake generates 1 developer policy warning (`Policy CMP0177 is not set: install() DESTINATION paths are normalized` at `_deps/lvgl-src/env_support/cmake/custom.cmake:66`), which is an upstream LVGL CMake script characteristic and not a project defect.

---

## 3. Two-Stage Rendering & Responsiveness Evidence

Responsiveness and display traffic benchmarks were re-measured during Check 39:

| Metric | Stage 1 (Tactile Card Highlight) | Stage 2 (Deferred Chart Render) | Accepted 2026-09-24 Baseline (Stage 1) |
| :--- | :---: | :---: | :---: |
| **Flushes Count** | **5 flushes** | **14 flushes** | 5 flushes |
| **Dirty Pixels** | **34,532 px** | **89,388 px** | 34,532 px |
| **State Flag** | `pending_chart = 1` | `pending_chart = 0` | `pending_chart = 1` |
| **Heap Drift (100 switches)** | \multicolumn{2}{c|}{**0 B drift** ($16,944\text{ B} \rightarrow 16,944\text{ B}$)} | 0 B drift |

**Analysis:**
- **Stage 1 (Tactile Card Highlight):** Exactly matches the accepted baseline of **5 flushes / 34,532 px**, delivering immediate visual feedback upon card touch without rendering the expensive chart synchronously.
- **Stage 2 (Chart Render):** Redraws the chart, Y ticks, and curve in **14 flushes / 89,388 px** during the next tick.
- **Memory Stability:** Zero heap drift ($16,944\text{ B} \rightarrow 16,944\text{ B}$) across 100 consecutive metric switches.

---

## 4. Host UI Component RAM Ledgers & Heap Safety

### Default Profile (42 KiB LVGL Heap)
- **Fixed Pool Size:** $43,008\text{ B}$ ($42\text{ KiB}$).
- **Draw Buffer:** $16,000\text{ B}$ (one partial $800 \times 10$ RGB565 line buffer).
- **Pool + Draw Subtotal:** $59,008\text{ B}$.
- **Static UI Data:** $4,704\text{ B}$ (`.data = 576 B`, `.bss = 4,128 B`).
- **Style RAM:** $0\text{ B}$ (all styles declared `const` in `.rodata` Flash).
- **Settled Pool Free:** $8,072\text{ B}$ minimum (Home page) with **0 B drift** over 50 stress cycles.
- **Contemporaneous Largest Free Block at Peak:** $6,584\text{ B}$.
- **Allocation-Level Transient Min Free:** $3,664\text{ B}$.
- **Allocation Failures:** $0$.

### Opt-in Constrained Profile (40 KiB LVGL Heap)
- **Fixed Pool Size:** $40,960\text{ B}$ ($40\text{ KiB}$).
- **Draw Buffer:** $16,000\text{ B}$ (one partial $800 \times 10$ RGB565 line buffer).
- **Pool + Draw Subtotal:** $56,960\text{ B}$.
- **Settled Pool Free:** **$5,984\text{ B}$** minimum (> $4,096\text{ B}$ safety threshold) with **0 B drift** over 50 stress cycles.
- **Contemporaneous Largest Free Block at Peak:** $4,856\text{ B}$.
- **Allocation-Level Transient Min Free:** $1,616\text{ B}$.
- **Allocation Failures:** $0$.

---

## 5. Visual Scenario Captures (30 Manifest Frames)

All 30 native $800 \times 480$ RGB565 scenario frames generated by `sim_pc.exe --shots` were converted to PNG in [`captures/`](./captures/) and verified:

| # | Frame File Name | View / State Description | SHA-256 Checksum |
| :-: | :--- | :--- | :--- |
| 1 | [`splash.png`](./captures/splash.png) | Boot splash screen with Smart Hub logo | `2073e1e9d8c6b1fd51562406150f56ec5982ee7f2e9a55474a7f8362f6c9c0a3` |
| 2 | [`home_good.png`](./captures/home_good.png) | Home page — Good IAQ (eCO₂ & TVOC nominal) | `2750f5437aa09737380616fed76edcaaeb84926c13b7460dfcaf3e143bc89e66` |
| 3 | [`home_moderate.png`](./captures/home_moderate.png) | Home page — Moderate IAQ | `ea67d1d50b678c2e53c56fba8a547c1b593db20bd38d32625d869c1e4be913d4` |
| 4 | [`home_poor.png`](./captures/home_poor.png) | Home page — Poor IAQ | `e5c87e99ef65b406d15e6a1e3348e8ab965afa7626a5b9cd570e9fda019d9c2b` |
| 5 | [`home_offline.png`](./captures/home_offline.png) | Home page — Node radio link offline | `cd15f7880ffd90c23814ba1b0df616f72f7c72a4e7bf2e1130770f6a63e915e7` |
| 6 | [`home_auto_paused_switch.png`](./captures/home_auto_paused_switch.png) | Home page — Auto paused on device switch | `3d6b52712abaa2591b3bcdf13ba5205588c80f9edc70e3438ce371f778d077e4` |
| 7 | [`home_sensor_fail.png`](./captures/home_sensor_fail.png) | Home page — Modbus sensor communication fault | `abe1b78508c69b1586ca6599b3210060da4621bdc545c70c5556b3ba01432975` |
| 8 | [`home_relay_pending.png`](./captures/home_relay_pending.png) | Home page — Relay toggle command pending ACK | `191327a3f10ab2c079800e738d308d8680e64367ff18bc819ab2e4dde33299f1` |
| 9 | [`home_relay_retry.png`](./captures/home_relay_retry.png) | Home page — Relay toggle command retry state | `ef87a6191e8501445c95b6b44db9291f934ca5a5d26fd5987229ca865bbfac49` |
| 10 | [`trends_data.png`](./captures/trends_data.png) | Trends page — eCO₂ selected with 16 samples | `ed8641e0c357949f2b9786c835ea910656947e223b8691639e239685de23b743` |
| 11 | [`trends_stage1_interim.png`](./captures/trends_stage1_interim.png) | Trends page — Stage 1 interim tactile highlight frame | `28b9ca0d0ee99fd2d900fad9f59a03e487115178ec6a1ae20395aa35eb5a13ff` |
| 12 | [`trends_voc.png`](./captures/trends_voc.png) | Trends page — TVOC selected with 16 samples | `7266c3cc49f1d3d24c68561707a7fa6fe8546f6eaa9353429a84ca3977af7e9f` |
| 13 | [`trends_temp.png`](./captures/trends_temp.png) | Trends page — Temperature selected with 16 samples | `b5d111cd38aa8ba8f97537d9649b6d7443bfefc1ef61e5e086881d63989aca77` |
| 14 | [`trends_humid.png`](./captures/trends_humid.png) | Trends page — Humidity selected with 16 samples | `9fbb508831deebda2c649df6d260e6cacc56f7bc0697300889e8e0fc34be106c` |
| 15 | [`trends_autonomous_partial.png`](./captures/trends_autonomous_partial.png) | Trends page — Autonomous history (6 samples, `-15 min … Now`) | `456281a1fec1bc6a4ab1d0736651af5daad415de31ee553746cce1745ed469bd` |
| 16 | [`trends_one_sample.png`](./captures/trends_one_sample.png) | Trends page — 1-sample state (`Collecting readings…` + hint) | `c6440808be4cf15e4ccad3dfa6e809e908b2d0bcfabf34a02bd186d680cbb59b` |
| 17 | [`trends_empty.png`](./captures/trends_empty.png) | Trends page — 0-sample empty state (`No readings yet`) | `95b9733627f1579d3cbea898c20b84235bd9ee1668d72a7e8474e84079ef6c4f` |
| 18 | [`devices_grid.png`](./captures/devices_grid.png) | Devices page — 4-appliance control grid | `5585c26b29592af4bc9726ebee3398a9de0cadd217d289db57dd3ee79185a08f` |
| 19 | [`devices_settings.png`](./captures/devices_settings.png) | Devices page — Settings tab view | `2436fc91a2f52eeb3e2cc7e4af7ff9aa6b436d3644fe5a3ab54323aec7b04d8c` |
| 20 | [`home_link_degraded.png`](./captures/home_link_degraded.png) | Home page — LoRa link degraded state | `a3388c46d7c913649b0a4d549156e980e257c1c70f6cd857a471202ade47374e` |
| 21 | [`home_auto_paused.png`](./captures/home_auto_paused.png) | Home page — Auto paused notification | `e5eb42f79347b2da4f9fb27734923c2be73b8f5580009111ede77fe980852ab0` |
| 22 | [`devices_auto_thresholds.png`](./captures/devices_auto_thresholds.png) | Devices page — Auto thresholds overview | `71ba8de43b5e3c04bf83d61b50a9de22785afe6faf6edd93b2449a73b623a797` |
| 23 | [`devices_settings_manual_only.png`](./captures/devices_settings_manual_only.png) | Devices page — Manual-only settings view | `5d9abc8cfed35b9612f3017cc0f6e1f2d3e7ad9dad106487ecbdd0c8015de183` |
| 24 | [`devices_auto_purifier.png`](./captures/devices_auto_purifier.png) | Devices page — Air Purifier Auto TVOC thresholds | `6e08248ac8d348b8afde40760d32785b7f85e1b21f8479b1fa596afd57d27b4b` |
| 25 | [`devices_auto_humidifier.png`](./captures/devices_auto_humidifier.png) | Devices page — Humidifier Auto RH thresholds | `848ccb9284e39fc34eaa9604b26c9a389ba75c54a881b38901bcf8ec818126fe` |
| 26 | [`devices_auto_max_60000.png`](./captures/devices_auto_max_60000.png) | Devices page — Auto threshold 60,000 ppm maximum | `214d019acdf961355da8e3b36b277caca8c77285569476596b41553d2f1d1186` |
| 27 | [`devices_auto_invalid_draft.png`](./captures/devices_auto_invalid_draft.png) | Devices page — Invalid Auto threshold draft warning | `1282dc75c1d66c083f24cc1b4815e43c3a60f4b6aff034686e7893101fa9fcbb` |
| 28 | [`home_temp_humid_invalid.png`](./captures/home_temp_humid_invalid.png) | Home page — Out-of-bounds temperature/humidity | `eba8275f7e5b1b72f04ece4d49754ac76bc6dbadaa8c8d23107d723145555ea4` |
| 29 | [`dropdown_open_top.png`](./captures/dropdown_open_top.png) | Devices page — Top dropdown menu open | `58fb01d351ed9affe3d4d3ac6386354fd73503462db3d31a585b55db8bc4f968` |
| 30 | [`dropdown_open_bottom.png`](./captures/dropdown_open_bottom.png) | Devices page — Bottom dropdown menu open | `4c4e4f316a53362afb226abf45cf9917c777949f533e942fc53491e30ce9f88a` |

---

## 6. Target-Only Limitations & Disclaimers

> [!WARNING]
> **Status on Physical Target Hardware:** **UNVERIFIED**  
> All host verification and regression suites pass on the Windows host. However, the following physical target hardware interactions remain **UNVERIFIED** until evaluated on the physical hardware with oscilloscope / logic analyzer / memory map analysis:
> 1. Physical LCD touch-to-photon latency (interrupt to photon display response).
> 2. Real I2C capacitive touch controller interrupt propagation delay and jitter.
> 3. Display controller GRAM tearing effect (`TE`) line synchronization and QSPI DMA FIFO overhead.
> 4. Physical MCU memory map, RTOS task stack margins, and hardware radio/DMA ring buffers under full firmware integration.
> 5. Actual 920 MHz LoRa RF packet transmission and propagation characteristics.

---

## 7. Delivery Status & Modified Files Ledger

### Modified Files Summary:
- **`ui.c`**: Implemented Trends B1–B4 refinements (hero card styling `ui_style_card_hero` with `LV_STATE_CHECKED`, celadon dot indicator, lowered title and chart geometry, raised Y unit, 15-min fixed window, round-number Y ticks with mathematical floor rounding), reset history test hook.
- **`sim_pc/main.c`**: Renamed `voc_index` $\rightarrow$ `tvoc_ppb`, updated fake Node for protocol v2 (`GET_STATUS` / `HUB_CMD`, `sample_seq` incrementing), expanded scenario generator to 30 shots including autonomous partial and one-sample states, added Check 41 for v2 protocol verification, updated Check 39 for revised Trends layout.
- **`sim_pc/ui_test_api.c` & `sim_pc/ui_test_api.h`**: Renamed `voc_index` $\rightarrow$ `tvoc_ppb`, updated test hooks for protocol v2 and Trends refinements, added `ui_test_reset_history()`.
- **`sim_pc/test_restore_integration.c`**: Renamed `voc_index` $\rightarrow$ `tvoc_ppb`.
- **`sim_pc/ui_test_hooks.h`**: Synchronized test hook structures with updated `ui.c` hooks.
- **`tools/run_regression.ps1`**: Updated exact manifest from 28 to 30 raw scenario shots.
- **`docs/UI_DESIGN_BRIEF.md`**: Updated 2026-09-25 Trends subsection and amended D-040.
- **`docs/user/SMART_HUB_UI_GUIDE.md`**: Updated §7.4 layout and autonomous history descriptions.
- **`docs/MCU_BASELINE.md`**: Updated status block with re-measured heap and benchmark numbers.
- **`README.md` & `docs/README.md`**: Updated top status lines.
- **`PLAN.md` & `DEV_LOG.md`**: Recorded completion of LoRa v2 and Trends refinements delivery.
- **`docs/prompts/README.md` & `docs/prompts/CURRENT.md`**: Updated status to `IMPLEMENTED — awaiting review`.
