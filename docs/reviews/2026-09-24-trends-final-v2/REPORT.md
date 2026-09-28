# Consolidated Delivery Report — 2026-09-24 Trends Final Delivery (v2)

**Date:** 2026-09-24  
**Scope:** Final consolidated resolution addressing [docs/prompts/CURRENT.md](../../prompts/CURRENT.md) and independent review findings from [docs/reviews/2026-09-24-trends-final-independent/REVIEW.md](../2026-09-24-trends-final-independent/REVIEW.md).  
**Status:** IMPLEMENTED — all 5 verification gates passed; ready for final review.

---

## 1. Executive Summary & Disposition

This consolidated delivery directly resolves every finding from the independent review ([`2026-09-24-trends-final-independent/REVIEW.md`](../2026-09-24-trends-final-independent/REVIEW.md)):

1. **P1 Visible Feedback on Normal Schedule:**
   - **Root Cause of 0 px in v1:** In v1, `on_metric_tab_click()` modified tab styling and set a pending flag, but did not guarantee that LVGL's refresh timer was ready to flush before `ui_tick()` was called in the normal application loop. Consequently, `ui_tick()` rendered Stage 2 in the same tick or bypassed intermediate flushing, logging 0 pixels for Stage 1.
   - **Resolution:**
     - `on_metric_tab_click()` now calls `lv_timer_ready(disp->refr_timer)` and sets `s_stage1_flush_pending = true`.
     - Added `ui_is_disp_busy_or_dirty()` in [ui.c](../../../ui.c) checking `disp->inv_p > 0`, `disp->rendering_in_progress`, and `disp->driver->draw_buf->flushing`. `ui_tick()` will NOT execute Stage 2 chart rasterization until Stage 1 has genuinely completed flushing.
     - **Verified Result on Normal Application Loop:** On both default (42 KiB) and constrained (40 KiB) regression runs, Stage 1 produces **5 flushes, 34,532 pixels** ($> 0$, $\le 6$ flushes, $\le 35\text{k px}$) strictly bounded to the left-rail tab cards `[19..212, 107..284]`.
2. **Interim Title Consistency:**
   - Decoupled `update_trends_title()` from `update_trends_tabs()`.
   - When a user taps a new metric card, the card highlights immediately in Stage 1, while the title label retains the **old** metric's name (e.g. "CO₂").
   - In Stage 2, `ui_render_trend_metric()` simultaneously updates the title (e.g. "VOC"), chart curve, Y-axis scaling, and min/max stats, eliminating visual inconsistency between title and graph.
3. **Delayed Asynchronous Flush / DMA Simulation:**
   - Upgraded [sim_pc/main.c](../../../sim_pc/main.c) CHECK 39 to simulate asynchronous DMA transfers by withholding `lv_disp_flush_ready()` on `driver->draw_buf->flushing_last`.
   - Proved that `ui_tick()` observes the busy/flushing state and does not trigger Stage 2 prematurely. When the simulated DMA completes (`sim_async_flush_complete()`), Stage 2 executes cleanly.
4. **Interaction Edge Cases Verified:**
   - Telemetry frame arrival during pending Stage 1 does not disrupt tactile highlight or pending state.
   - Link loss / recovery while Stage 1 is pending behaves deterministically.
   - Trends tooltip hides immediately on tab card tap.
   - Navigating away from Trends (Page exit) and re-entering cancels pending chart and renders the current page cleanly.
   - Tapping the currently active metric is a no-op (0 flushes, 0 pixels).
5. **Calculated QSPI Bus Latency & Hardware Candor:**
   - Bus transfer timings at the declared $20\text{ MB/s}$ QSPI bandwidth are logged:
     - Stage 1: **3.45 ms** ($69.1\text{ KB}$, 34,532 px).
     - Stage 2: **10.58–11.38 ms** ($211.7\text{–}227.7\text{ KB}$, 105,843–113,849 px).
   - **Target Hardware Candor:** Physical LCD touch-to-photon latency and real DMA transfer times are explicitly marked **UNVERIFIED** on the host simulator.
6. **P2 Safe Verifier Process Cleanup:**
   - In [tools/verify_production.ps1](../../../tools/verify_production.ps1), added try/catch around CIM enumeration with fail-closed watchdog error reporting.
   - Enforced strict `$ExpectedStartTime` verification ($> \text{MinValue}$) in `Terminate-OwnedProcessTree`, outright rejecting raw PID termination.
   - Enhanced Fixture 6 in `Invoke-ProcessSafetyFixtures` to test and confirm rejection of missing/unspecified start times.
7. **P2 Table-Driven Combined Fault Matrix (CHECK 40):**
   - Implemented a genuinely table-driven sequence (`fault_matrix_step_t matrix_steps[]`) covering Steps 0 to 5.
   - All 4 Trends metrics (Current, Min, Max, valid/gap status) asserted across every step.
   - Unaffected Temperature and Humidity history samples confirmed valid (`valid = true`) during CO₂/VOC dual faults.
   - All 3 Auto editors (Purifier, Fan, Humidifier) verified with strictly scoped label checks (`*h40->dialog_live_lbl`).
   - Verified 10s anti-short-cycle hold, 3-frame qualification, wire `active_low` GPIO bit 1 == 0, node echo GPIO bit 1 == 0, and VOC=0 valid baseline reading.
8. **P2 RAM & Baseline Documentation Reconciliation:**
   - Reconciled [docs/MCU_BASELINE.md](../../../docs/MCU_BASELINE.md): actual static draw buffer is **16,000 B** ($800 \times 10 \times 2\text{ B}$), 40 KiB settled min is **6,272 B** ($+104\text{ B}$ drift), transient min free is **1,896 B**.
   - Added explicit disclaimers regarding host-only GNU wrap instrumentation and target hardware latency.
9. **Visual & Behavioral Invariants:**
   - 100% frozen Quiet Architecture appearance. All visible text $\ge 18\text{px}$.
   - Worktree integrity: dirty worktree preserved; 0 commits, 0 pushes.

---

## 2. Verification Gates Summary

All five verification gates were executed cleanly on the Windows host:

| Gate | Command | Result | Diagnostic / Output Summary |
| :--- | :--- | :---: | :--- |
| **1. Simulator Build** | `tools/build_sim.bat` | **PASS** | 0 errors, 0 warnings. Binary: `sim_pc/build/sim_pc.exe` |
| **2. Regression Suite (Default)** | `sim_pc/build/sim_pc.exe --regression` | **PASS** | **40/40 checks passed (100%)**, Stage 1: 5 flushes, 34,532 px |
| **2b. Regression Suite (40 KiB)** | `build-40k/sim_pc.exe --regression` | **PASS** | **40/40 checks passed (100%)**, Stage 1: 5 flushes, 34,532 px |
| **3. Production Verifier** | `tools/verify_production.bat` | **PASS** | Phases 1–4 passed; process ownership safety fixtures passed |
| **4. Asset Generator** | `python tools/generate_assets.py --check` | **PASS** | 25 canonical masters, 68 runtime icons verified |
| **5. MCU Export Manifest** | `tools/export_mcu.bat --verify` | **PASS** | 16 MCU bundle files verified with SHA-256 manifest |

Raw execution logs are saved permanently in this review directory:
- [`build_sim.log`](./build_sim.log)
- [`regression_default.log`](./regression_default.log)
- [`regression_40k.log`](./regression_40k.log)
- [`verify_production.log`](./verify_production.log)
- [`generate_assets.log`](./generate_assets.log)
- [`export_mcu.log`](./export_mcu.log)

---

## 3. P1 Responsiveness: Two-Stage Rendering & Measurements

### 3.1 Architectural Resolution of 0-Pixel Stage 1

In v1, the touch event handler updated tab styles but relied on the next periodic display refresh timer. When run inside the regression loop, `ui_tick()` ran before the refresh timer fired or was measured without intermediate display flushing.

In v2, the flow is strictly enforced:
```text
User Tap on Metric Tab Card (on_metric_tab_click)
  ├── 1. Update tab card active/focus styles
  ├── 2. Hide any active tooltip
  ├── 3. Set s_stage1_flush_pending = true, s_pending_trend_chart = true
  ├── 4. lv_timer_ready(disp->refr_timer)  <-- Forces display timer ready
  └── 5. Return immediately (< 0.1 ms)
            │
            ▼
Application Loop (lv_timer_handler)
  ├── Stage 1 Flushes Execute (5 flushes, 34,532 px)
  ├── Left-rail cards update physically on screen
  └── s_stage1_flush_pending cleared by flush_cb / ui_tick
            │
            ▼
Application Loop (ui_tick)
  ├── Guard: ui_is_disp_busy_or_dirty() == false
  ├── Invokes ui_render_trend_metric(s_pending_trend_metric)
  ├── Stage 2: Title committed synchronously with Chart, Y-axis, Stats
  └── s_pending_trend_chart cleared
```

### 3.2 Regression Suite Measured Results (CHECK 39)

From [`regression_default.log`](./regression_default.log) and [`regression_40k.log`](./regression_40k.log):
```text
[CHECK 39] Trends Metric Switching Latency & Redraw Benchmark (P1)...
  (Benchmark) 100 metric switches: 2524 flushes, 15181555 pixels pushed, heap_free: 13360 -> 13360
  (Two-Stage) Stage 1 (Tactile Card): 5 flushes, 34532 pixels, pending_chart=1
  (Two-Stage) Stage 2 (Chart Render): 19 flushes, 113849 pixels, pending_chart=0
  (Simulated QSPI 20 MB/s Timing): Stage 1 = 3.45 ms (69.1 KB), Stage 2 = 11.38 ms (227.7 KB)
  NOTE: Real target LCD touch-to-photon latency is UNVERIFIED on host simulator.
  PASS: Trends metric switching latency, two-stage response, coalescing and stability verified.
```

### 3.3 Standalone Benchmark Fixture Results

Compiled and executed [`benchmark_fixture.c`](./benchmark_fixture.c) producing [`benchmark_results.txt`](./benchmark_results.txt):

```text
====================================================================
 Two-Stage Tactile Feedback Measurement (User Tap on VOC card)
====================================================================
Stage 1 (Immediate Tactile Highlight):
  pixels      : 34532 px (69064 bytes)
  flushes     : 5 flushes (bounded <= 6)
  rect        : [19..212, 107..284] (Left-rail tab cards only!)
  pending     : TRUE (Chart deferred)
  title       : 'CO₂' (Interim title preserves old plot context)
  sim_qspi_ms : 3.45 ms (at declared 20 MB/s bus bandwidth)

Stage 2 (Deferred Chart & Stats Rasterization in ui_tick):
  pixels      : 105843 px (211686 bytes)
  flushes     : 18 flushes
  rect        : [221..761, 107..419] (Chart area and stats)
  pending     : FALSE (Cleared)
  title       : 'VOC' (Title committed synchronously with new chart)
  sim_qspi_ms : 10.58 ms (at declared 20 MB/s bus bandwidth)

Total Single Switch Cost: 140375 px, 23 flushes, 14.04 ms total simulated bus time
NOTE: Real LCD touch-to-photon latency is UNVERIFIED on host simulator.

====================================================================
 Rapid Tap Coalescing Benchmark (Burst: VOC -> TEMP -> HUMIDITY)
====================================================================
Uncoalesced (3 full chart renders) : 451181 px, 75 flushes
Coalesced (3 tactile + 1 chart)    : 214174 px, 34 flushes
Savings from Burst Coalescing      : 237007 px (-52.5% pixels), 41 flushes (-54.7% flushes)
NOTE: The ~46% reduction applies exclusively to rapid bursts; single switches render fully.

====================================================================
 100-Tap Rapid Switching Stress & Memory Stability
====================================================================
  100 switches : 6311782 px, 986 flushes
  Heap before  : 13752 B free (largest 12464 B)
  Heap after   : 13752 B free (largest 12464 B)
  Heap drift   : 0 B (Zero drift verified!)
```

### 3.4 Target Hardware Latency Candor & Oscilloscope Plan

> [!WARNING]
> **Status on Target Hardware:** **UNVERIFIED**  
> Simulator measurements calculate bus transmission timings at a nominal $20\text{ MB/s}$ QSPI clock ($3.45\text{ ms}$ Stage 1, $10.58\text{–}11.38\text{ ms}$ Stage 2). However, real physical touch-to-photon latency, I2C touch interrupt latency, DMA FIFO overhead, and display controller GRAM tearing effect (`TE`) must be measured on the physical board.

**Target Hardware Oscilloscope / Logic Analyzer Instrumentation Guide:**
1. **Channel 1 (Touch Input):** Connect to `TOUCH_INT` (falling edge trigger).
2. **Channel 2 (Stage 1 Complete):** Set a test GPIO high upon entry into `on_metric_tab_click` and clear it when the last partial flush for Stage 1 completes (`lv_disp_flush_ready`). Target: $\Delta t < 15\text{ ms}$.
3. **Channel 3 (Stage 2 Complete):** Set a test GPIO high upon entry into `ui_render_trend_metric` and clear it when the final chart flush completes. Target: $\Delta t < 60\text{ ms}$.
4. **Channel 4 (QSPI CS):** Monitor QSPI Chip Select line to verify that burst taps trigger only left-rail updates until the user settles on a selection.

---

## 4. P2 Safe Verifier Process Cleanup

In [`tools/verify_production.ps1`](../../../tools/verify_production.ps1):

1. **Fail-Closed on CIM Enumeration Exceptions:**
   - In both `Invoke-ProcessWithTimeout` and `Invoke-ConcurrentWorkers`, wrapped `Get-CimInstance` calls in strict try/catch blocks.
   - If CIM enumeration fails (e.g. WMI service pause, access denial, query failure), the watchdog reports an explicit diagnostic error and throws rather than silently ignoring children.
2. **Mandatory StartTime Validation:**
   - In `Terminate-OwnedProcessTree`, added explicit parameter validation:
     ```powershell
     if ($ExpectedStartTime -le [datetime]::MinValue) {
         Write-Warning "Root PID $RootPid termination rejected: ExpectedStartTime is mandatory for verified task cleanup"
         return
     }
     ```
   - Outright rejects any attempt to kill processes by raw numeric PID alone.
3. **Safety Fixtures Passing:**
   - Tested all 6 process safety fixtures in Phase 1:
     - Normal child task completion.
     - Timed-out task tree cleanup.
     - Recycled root PID protection (StartTime mismatch rejected).
     - Recycled descendant PID protection (lineage break rejected).
     - Unrelated process protection (System PID 4 / Explorer protected).
     - Fail-closed error handling (missing/unspecified StartTime rejected).

---

## 5. P2 Table-Driven Combined Fault Matrix (CHECK 40)

CHECK 40 in [`sim_pc/main.c`](../../../sim_pc/main.c) executes a table-driven 6-step test verifying the complete operational matrix:

```text
[CHECK 40] Table-Driven Combined Sensor-Fault & Partial-Recovery Matrix (E2)...
  [Step 0: Baseline All Healthy] Executing matrix verification...
  [Step 1: Combined Fault (CO2 & VOC invalid, Temp & Humid valid)] Executing matrix verification...
  [Step 2: 1-Min Autonomous Gap Progression] Executing matrix verification...
  [Step 3: Partial Recovery (CO2=1200 valid, VOC invalid)] Executing matrix verification...
  [Step 4: VOC Valid Zero Baseline (VOC=0 valid)] Executing matrix verification...
  [Step 5: Full Recovery All Sensors Nominal] Executing matrix verification...
  PASS: Combined fault & partial recovery matrix, qualification, active_low and VOC=0 verified.
```

### Table of Asserted Fault & Recovery States

| Step | State Description | CO₂ Status | VOC Status | Temp & Humid Status | Fan Relay Wire | Purifier Relay Wire | Auto Editor Live Scoped Labels | Trends History Integrity |
| :---: | :--- | :---: | :---: | :---: | :---: | :---: | :--- | :--- |
| **0** | **Baseline All Healthy** | $600\text{ ppm}$ (valid) | $80$ (valid) | Valid ($23.5^\circ\text{C}$, $46\%$) | Auto (GPIO 1) | Auto (GPIO 1) | Fan: "600", Purifier: "80", Humid: "46" | 16 valid samples across all 4 metrics |
| **1** | **Combined Dual Fault** | Offline timeout (`valid=false`) | `0xFFFF` fault (`valid=false`) | **Valid** ($23.5^\circ\text{C}$, $46\%$) | Safe Off (GPIO 1) | Safe Off (GPIO 1) | Fan: "--", Purifier: "--", Humid: "46" | CO₂/VOC current="--", Temp/Humid valid values |
| **2** | **1-Min Autonomous Gap** | Offline gap (`valid=false`) | `0xFFFF` gap (`valid=false`) | **Valid** ($23.5^\circ\text{C}$, $46\%$) | Safe Off (GPIO 1) | Safe Off (GPIO 1) | Fan: "--", Purifier: "--", Humid: "46" | Slot 1: CO₂ & VOC `valid=false`; Temp & Humid `valid=true` |
| **3** | **Partial Recovery (Hold & Qual)** | $1200\text{ ppm}$ (`valid=true`) | `0xFFFF` fault (`valid=false`) | Valid ($23.5^\circ\text{C}$, $46\%$) | **Active ON (GPIO 0)** | Safe Off (GPIO 1) | Fan: "1200", Purifier: "--", Humid: "46" | 10s hold suppresses relay; 3 fresh frames qualify ON |
| **4** | **VOC Valid Zero Baseline** | $800\text{ ppm}$ (valid) | **$0\text{ index}$ (valid)** | Valid ($23.5^\circ\text{C}$, $46\%$) | Auto Off (GPIO 1) | Auto Off (GPIO 1) | Fan: "800", Purifier: "0", Humid: "46" | VOC=0 index evaluates as valid, not fault |
| **5** | **Full Recovery Nominal** | $600\text{ ppm}$ (valid) | $80$ (valid) | Valid ($23.5^\circ\text{C}$, $46\%$) | Auto (GPIO 1) | Auto (GPIO 1) | Fan: "600", Purifier: "80", Humid: "46" | All banners clean, all metrics valid |

- **Strict Scoping:** All Auto editor assertions verify the target live label (`*h40->dialog_live_lbl`), ensuring no cross-widget confusion.
- **Wire Polarity:** Wire bit 1 == 0 (`active_low`) and node status readback GPIO bit 1 == 0 confirmed for energizing the Fan relay in Step 3.

---

## 6. P2 RAM & Baseline Documentation Reconciliation

In [`docs/MCU_BASELINE.md`](../../../docs/MCU_BASELINE.md):

1. **Draw Buffer Memory Footprint:**
   - Corrected to **16,000 B** ($800 \times 10 \text{ lines} \times 2\text{ B/px}$).
2. **Settled Min Free & Drift:**
   - **42 KiB Baseline:** Settled Min Free = **8,296 B**, Largest Block = **7,176 B**, Transient Min Free = **3,944 B**, Drift = **0 B**.
   - **40 KiB Constrained Profile:** Settled Min Free = **6,272 B**, Largest Block = **5,128 B**, Transient Min Free = **1,896 B**, Drift = **+104 B**.
3. **Disclaimers Added:**
   - Explicitly clarified that GNU linker wrapping (`-Wl,--wrap=lv_mem_alloc`) is host simulator instrumentation only and does not exist in target firmware.
   - Declared that target touch-to-photon latency is UNVERIFIED on host simulator and requires oscilloscope measurement.

---

## 7. Delivery Artifacts & Cryptographic Signatures

### 7.1 SHA-256 Signatures

| File | SHA-256 Hash |
| :--- | :--- |
| [`ui.c`](../../../ui.c) | `f74723d8c41ef3cd5b9b783412068bacd8749b5e834a69809728af503c6ed12e` |
| [`ui.h`](../../../ui.h) | `d1130a23484a2e00462755bcd5b7a007c958cb1a3b1a51392e918cf00574b6e2` |
| [`ui_types.h`](../../../ui_types.h) | `5a3952d632fdfb852bfaf4da761e4f7fa3074bea762df3a2608be462d4f24715` |
| [`sim_pc/ui_test_hooks.h`](../../../sim_pc/ui_test_hooks.h) | `4989255f21b415c8fc43acadb715cc6dc352dfc637202603c319ad2b20ae135a` |
| [`sim_pc/CMakeLists.txt`](../../../sim_pc/CMakeLists.txt) | `be3e7c84daa1c77178be4c3df79a9241a1f62ca2f6563006c30d55f910afacec` |
| [`sim_pc/main.c`](../../../sim_pc/main.c) | `3dceff89a0864755676110ad58358a2e93ff48a4c46496058ab560deffee546e` |
| [`tools/verify_production.ps1`](../../../tools/verify_production.ps1) | `9b6a10d0241b8eaa692ab37c4d1fe8b688de6325175f4522193fdbaf5025e820` |
| [`docs/MCU_BASELINE.md`](../../../docs/MCU_BASELINE.md) | `166368b4829c21b2e5805fcab6f89d789cfaf5467e7c7bf34efa5afd03b3e238` |
| [`sim_pc/build/sim_pc.exe`](../../../sim_pc/build/sim_pc.exe) | `ee1ffc39098201c98919555955508d1229c041a41722083fe856185bb3c67c25` |
| [`build-40k/sim_pc.exe`](../../../build-40k/sim_pc.exe) | `377b05942167c9d6b5270a2fc286210a5509f93653591100011b773e4fdc5ed2` |
| [`benchmark_fixture.c`](./benchmark_fixture.c) | `93fe5068d6c48d46829baca36892a0da4e8a405e688ce64a8db61550ec8e83fe` |
| [`benchmark_fixture.exe`](./benchmark_fixture.exe) | `87ffc7c3e176f2f9b94dd5e88a98532812a8e85459c0d310c8ace56888220705` |
| [`benchmark_results.txt`](./benchmark_results.txt) | `93b29770501fb38be9cda52d7451076e1fed5346eb89fa8e3e97cfcd46dbe3cd` |

### 7.2 Captured Visual Evidence

Key UI scenarios captured during regression testing are saved in [`captures/`](./captures/):
- **Trends Screens:** [`trends_data.png`](./captures/trends_data.png) (CO₂ populated), [`trends_voc.png`](./captures/trends_voc.png), [`trends_temp.png`](./captures/trends_temp.png), [`trends_humid.png`](./captures/trends_humid.png), [`trends_empty.png`](./captures/trends_empty.png).
- **Home States:** [`home_good.png`](./captures/home_good.png), [`home_moderate.png`](./captures/home_moderate.png), [`home_poor.png`](./captures/home_poor.png), [`home_sensor_fail.png`](./captures/home_sensor_fail.png), [`home_offline.png`](./captures/home_offline.png), [`home_auto_paused.png`](./captures/home_auto_paused.png), [`home_temp_humid_invalid.png`](./captures/home_temp_humid_invalid.png).
- **Devices & Auto:** [`devices_grid.png`](./captures/devices_grid.png), [`devices_settings.png`](./captures/devices_settings.png), [`devices_auto_thresholds.png`](./captures/devices_auto_thresholds.png), [`devices_auto_purifier.png`](./captures/devices_auto_purifier.png), [`devices_auto_humidifier.png`](./captures/devices_auto_humidifier.png), [`devices_auto_max_10000.png`](./captures/devices_auto_max_10000.png), [`devices_auto_invalid_draft.png`](./captures/devices_auto_invalid_draft.png).

---

## 8. Conclusion

Every defect, ambiguity, and testing gap identified in the review has been addressed and verified:
1. **Positive Stage 1 tactile response** is proven on the normal application loop schedule (5 flushes, 34,532 px).
2. **Interim title consistency** preserves previous context during Stage 1 and commits simultaneously with chart rasterization in Stage 2.
3. **Asynchronous DMA flush simulation** proves `ui_tick()` does not prematurely execute while a flush is in progress.
4. **Table-driven fault matrix (CHECK 40)** verifies all 4 metrics across Steps 0–5, including unaffected metric sample retention, scoped Auto live labels, anti-short-cycle hold, qualification, wire active_low polarity, and VOC=0 baseline.
5. **Verifier process tree cleanup** fails closed on CIM enumeration failure and strictly requires valid StartTime.
6. **RAM baseline documentation** reflects reconciled draw buffer ($16,000\text{ B}$), 40 KiB settled min ($6,272\text{ B}$), and explicit disclaimers for host wrapping and unverified target LCD latency.
