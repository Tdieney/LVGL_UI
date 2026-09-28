# Consolidated Delivery Report — 2026-09-24 Trends Final Delivery

**Date:** 2026-09-24  
**Scope:** Final consolidated resolution addressing [docs/prompts/CURRENT.md](../../prompts/CURRENT.md) and independent review findings from [docs/reviews/2026-09-24-trends-delivery-independent/REVIEW.md](../2026-09-24-trends-delivery-independent/REVIEW.md).  
**Status:** IMPLEMENTED — all 5 verification gates passed; ready for final review.

---

## 1. Executive Summary & Disposition

This delivery resolves all remaining findings from the independent review in a single, consolidated turn:

1. **P1 Hardware Responsiveness & Perceived Latency:**
   - Implemented **Two-Stage Feedback-First Rendering**:
     - **Stage 1 (Immediate Tactile Feedback):** When a user taps a Trends tab card, the left-rail card selection state, focus styling, and active highlight update immediately (`< 5 ms`, strictly $\le 6$ flushes, $\approx 29\text{k}$–$42\text{k px}$). The chart rasterization is deferred.
     - **Stage 2 (Deferred Chart Rasterization):** The heavy chart curve recalculation, Y-axis scaling, and min/max statistics are rendered cooperatively in application context during `ui_tick()`.
   - Implemented **Rapid-Tap Burst Coalescing**:
     - Fast consecutive taps (e.g. VOC $\to$ TEMP $\to$ HUMIDITY before the next frame) coalesce. Intermediate heavy chart rasterizations are skipped, saving **209,079 px (-46.2%)** and **36 flushes (-48.0%)** on display bus traffic.
   - Validated durably in **CHECK 39** (regression suite) and a permanent standalone benchmark fixture [`benchmark_fixture.c`](./benchmark_fixture.c).
   - **Target Hardware Candor:** Simulator traffic reductions and two-stage timing are rigorously measured, but physical LCD touch-to-photon latency and QSPI DMA transfer times remain marked **UNVERIFIED** on real hardware. An oscilloscope/logic-analyzer instrumentation plan is supplied for the target board.

2. **P1 Safe Verifier Process Cleanup:**
   - Completely removed the unverified numeric fallback parameter `KnownDescendantPids` in [`tools/verify_production.ps1`](../../../tools/verify_production.ps1).
   - In `Get-VerifiedProcessTree`, added lineage revalidation during child traversal and made CIM enumeration exceptions fail closed with explicit diagnostic errors.
   - In `Invoke-ProcessSafetyFixtures`, replaced all references to System PID 4 and `explorer.exe` with owned mock test child processes (`powershell.exe -NoProfile -Command "Start-Sleep -Seconds 10"`).

3. **Carried E1 RAM Proof:**
   - Added GNU linker wrap hooks (`-Wl,--wrap=lv_mem_alloc -Wl,--wrap=lv_mem_realloc`) capturing allocation-level largest single request (**1,168 B**), allocation failure count (**0**), and peak transient memory usage.
   - Upgraded **CHECK 31** to record contemporaneous largest free block at the exact moment minimum total free pool size occurs:
     - **42 KiB Baseline:** Settled Min Free = **8,296 B** (contemporaneous largest block = **7,176 B**); Render Transient Min Free = **3,944 B**; Drift = **0 B**.
     - **40 KiB Constrained Profile:** Settled Min Free = **6,248 B** (contemporaneous largest block = **5,128 B**); Render Transient Min Free = **1,896 B**; Drift = **0 B**.
   - Updated [`docs/MCU_BASELINE.md`](../../../docs/MCU_BASELINE.md) top section with September 24 verified figures and target MCU firmware RAM disclaimer.

4. **Carried E2 Combined Fault Matrix:**
   - Implemented durable, table-driven **CHECK 40** in [`sim_pc/main.c`](../../../sim_pc/main.c) testing combined simultaneous faults:
     - Dual simultaneous failure (CO₂ offline timeout + VOC `0xFFFF` sensor fault) across Home, Trends, and Auto editors.
     - Unaffected metric history retention (Temperature and Humidity continuous and valid).
     - 1-minute autonomous history gap logging (`valid = false`).
     - Partial recovery with 10-second anti-short-cycle hold and 3-frame qualification.
     - `active_low` wire polarity mapping verification (GPIO bit 1 == 0 for ON relay).
     - Node echo/readback verification.
     - Valid zero baseline reading (`VOC = 0`).
     - Full recovery to clean operational state.

5. **Visual & Behavioral Invariants:**
   - **Quiet Architecture Visuals:** 100% preserved. 0 layout shifts, 0 font size changes (all visible text $\ge 18\text{px}$).
   - **Worktree Integrity:** Dirty worktree preserved; 0 commits, 0 pushes.

---

## 2. Verification Gates Summary

All five verification gates were executed cleanly on the Windows host:

| Gate | Command | Result | Diagnostic / Output |
| :--- | :--- | :---: | :--- |
| **1. Simulator Build** | `tools/build_sim.bat` | **PASS** | 0 errors, 0 warnings. Binary: `sim_pc/build/sim_pc.exe` |
| **2. Regression Suite** | `tools/run_regression.bat` | **PASS** | **40/40 checks passed (100%)**, smoke passed, 27 raw captures |
| **2b. 40 KiB Profile** | `build-40k/sim_pc.exe --regression` | **PASS** | **40/40 checks passed (100%)**, Check 31 pass |
| **3. Production Verifier** | `tools/verify_production.bat` | **PASS** | Phases 1–4 passed; process ownership fixtures passed |
| **4. Asset Generator** | `python tools/generate_assets.py --check` | **PASS** | 25 canonical masters, 68 runtime icons verified |
| **5. MCU Export Manifest** | `tools/export_mcu.bat --verify` | **PASS** | 16 MCU bundle files verified with SHA-256 manifest |

Raw execution logs are saved permanently in this directory:
- [`build_sim.log`](./build_sim.log)
- [`regression_default.log`](./regression_default.log)
- [`regression_40k.log`](./regression_40k.log)
- [`verify_production.log`](./verify_production.log)
- [`generate_assets.log`](./generate_assets.log)
- [`export_mcu.log`](./export_mcu.log)

---

## 3. P1 Responsiveness: Two-Stage Architecture & Measurements

### 3.1 Architecture

On physical microcontrollers with display GRAM over QSPI (e.g. ST7701S / ILI9806), redrawing the Trends chart involves transferring $\approx 150\text{k px}$ ($300\text{k B}$) across 24–26 partial flushes. When the touch callback synchronously plots the chart, the touch interrupt and UI loop freeze until all flushes complete, creating perceived touch sluggishness.

**The Solution:**
1. **Stage 1 (Touch Callback):**
   - Updates left-rail tab card visual highlight (`s_trends_tab_cards`).
   - Sets `s_pending_trend_chart = true` and `s_pending_trend_metric = metric`.
   - Hides any open tooltip.
   - Refreshes only the tab card column ($< 5\text{ms}$ host time, $\le 6$ flushes, $\approx 29\text{k}$–$42\text{k px}$). The user immediately perceives the button depress/selection.
2. **Stage 2 (`ui_tick()`):**
   - If `s_pending_trend_chart` is asserted and the current screen is Trends, `ui_tick()` invokes `ui_render_trend_metric(s_pending_trend_metric)` and clears the flag.
   - Heavy chart rasterization, Y-axis relabeling, and statistics run in normal application context without stalling touch responsiveness.
3. **Rapid-Tap Coalescing:**
   - If a user rapidly taps multiple tabs before the next `ui_tick()` (e.g. VOC $\to$ TEMP $\to$ HUMIDITY), each tap updates the card highlight immediately, but `s_pending_trend_metric` simply overwrites the target.
   - `ui_tick()` renders only the latest metric (HUMIDITY), skipping intermediate full-chart rasterizations.

### 3.2 Permanent Benchmark Fixture Results

Compiled and executed [`benchmark_fixture.c`](./benchmark_fixture.c) against current UI:

```text
====================================================================
 Two-Stage Tactile Feedback Measurement (User Tap on VOC card)
====================================================================
Stage 1 (Immediate Tactile Highlight):
  pixels   : 42,080 px (84,160 bytes)
  flushes  : 6 flushes (bounded <= 6)
  rect     : [19..339, 107..284] (Left-rail tab cards only!)
  pending  : TRUE (Chart deferred)
  host_cpu : 0.00 ms (Tactile response completed instantly)

Stage 2 (Deferred Chart & Stats Rasterization in ui_tick):
  pixels   : 98,718 px (197,436 bytes)
  flushes  : 17 flushes
  rect     : [221..761, 173..419] (Chart area and stats)
  pending  : FALSE (Cleared)
  host_cpu : 0.00 ms

====================================================================
 Rapid Tap Coalescing Benchmark (Burst: VOC -> TEMP -> HUMIDITY)
====================================================================
Uncoalesced (3 full chart renders) : 452,218 px, 75 flushes, 1.00 ms
Coalesced (3 tactile + 1 chart)    : 243,139 px, 39 flushes, 0.00 ms
Savings from Coalescing            : 209,079 px (-46.2% pixels), 36 flushes (-48.0% flushes)

====================================================================
 100-Tap Rapid Switching Stress & Memory Stability
====================================================================
  100 switches : 14,968,375 px, 2,500 flushes, 32.00 ms total CPU
  Heap before  : 13,768 B free (largest 12,480 B)
  Heap after   : 13,768 B free (largest 12,480 B)
  Heap drift   : 0 B (Zero drift verified!)
```

### 3.3 Five Trends States Baseline

| State | Metric | Pixels | Bytes (RGB565) | Flushes | Bounded Rect | Host CPU |
| :--- | :--- | :---: | :---: | :---: | :---: | :---: |
| **1. Empty Trends** (0 pts) | CO₂ (initial) | 0 | 0 | 0 | None (no-op) | 0.0 ms |
| | VOC | 52,917 | 105,834 | 8 | [19..367, 107..284] | 0.0 ms |
| | Temp | 60,844 | 121,688 | 10 | [19..473, 107..372] | 0.0 ms |
| | Humid | 60,844 | 121,688 | 10 | [19..473, 107..460] | 1.0 ms |
| **2. Singleton Trends** (1 pt) | CO₂ | 154,706 | 309,412 | 26 | [19..761, 107..460] | 0.0 ms |
| | VOC | 149,217 | 298,434 | 24 | [19..761, 107..419] | 0.0 ms |
| | Temp | 156,809 | 313,618 | 26 | [19..761, 107..419] | 0.0 ms |
| | Humid | 156,809 | 313,618 | 26 | [19..761, 107..460] | 1.0 ms |
| **3. Fully Populated** (16 pts)| CO₂ | 65,593 | 131,186 | 13 | [19..721, 107..460] | 0.0 ms |
| | VOC | 60,786 | 121,572 | 11 | [19..721, 107..284] | 0.0 ms |
| | Temp | 90,685 | 181,370 | 17 | [19..715, 107..419] | 0.0 ms |
| | Humid | 90,685 | 181,370 | 17 | [19..715, 107..460] | 1.0 ms |
| **4. Invalid-Gap Trends** | (Identical bounding box and flushes to Populated; gap segments correctly omitted) |
| **5. Live Autonomous Tick** | CO₂ update | 111,056 | 222,112 | 21 | [221..772, 119..449] | 1.0 ms |

### 3.4 Target Hardware Candor & Instrumentation Plan

> [!WARNING]
> **Status on Physical Hardware:** **UNVERIFIED**  
> While host measurements prove a 46.2% reduction in display traffic on burst taps and sub-millisecond tactile card selection, true physical touch-to-photon latency depends on the target MCU's touch controller (I2C interrupt latency), CPU clock, QSPI DMA transfer rate, and display GRAM tearing effect (`TE`).

**Recommended Oscilloscope / Logic Analyzer Instrumentation Plan for Target Board:**
1. **Channel 1 (Touch Input):** Connect to the touch controller interrupt pin (`TOUCH_INT`). Trigger on falling edge (touch assertion).
2. **Channel 2 (Stage 1 Complete):** Set a test GPIO high upon entry into `on_metric_tab_click` and clear it when the last partial flush for Stage 1 completes (`lv_disp_flush_ready`). Target: $\Delta t < 15\text{ ms}$.
3. **Channel 3 (Stage 2 Complete):** Set a test GPIO high upon entry into `ui_render_trend_metric` and clear it when the final chart flush completes. Target: $\Delta t < 60\text{ ms}$.
4. **Channel 4 (QSPI CS):** Monitor QSPI Chip Select activity to confirm no redundant flushes occur during rapid taps.

---

## 4. P1 Safe Verifier Process Cleanup

In [`tools/verify_production.ps1`](../../../tools/verify_production.ps1):

1. **Removed `KnownDescendantPids`:** Eliminated the numeric PID array parameter entirely. Cleanup now strictly requires verified process object records with original immutable creation timestamps (`StartTime`).
2. **Lineage Revalidation & Fail Closed:**
   - In `Get-VerifiedProcessTree`, parent identity is checked at each traversal hop.
   - CIM query exceptions now write an explicit error diagnostic and throw rather than silently swallowing errors, ensuring fail-closed safety.
3. **Safe Fixture Testing:**
   - In `Invoke-ProcessSafetyFixtures`, replaced references to System PID 4 and `explorer.exe` with owned mock test child processes spawned via `powershell.exe -NoProfile -Command "Start-Sleep -Seconds 10"`.
   - Verified that all 6 safety fixtures pass (normal completion, timeout, recycled root, recycled descendant, unrelated process protection, and fail-closed error handling).

---

## 5. Carried E1 RAM Evidence

### 5.1 Memory Allocation Linker Wrapping

In [`sim_pc/CMakeLists.txt`](../../../sim_pc/CMakeLists.txt), the simulator was linked with `-Wl,--wrap=lv_mem_alloc -Wl,--wrap=lv_mem_realloc`.  
In [`sim_pc/main.c`](../../../sim_pc/main.c), wrapper functions intercept every LVGL heap allocation:
- **Largest Single Request:** **1,168 B** (allocated by LVGL for table/chart buffers).
- **Allocation Failure Count:** **0** (no allocations ever failed across 50 stress cycles).
- **Contemporaneous Tracking:** Captured the largest free block at the exact moment minimum total free pool size was reached during render transients.

### 5.2 CHECK 31 Stress Test Results

| Profile | Pool Size | Settled Min Free | Contemporaneous Largest Block | Allocation Transient Min Free | Settled Drift | Result |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: |
| **Default Profile** | 42 KiB (43,008 B) | **8,296 B** | **7,176 B** | **3,944 B** | **+0 B** | **PASS** |
| **Constrained Profile** | 40 KiB (40,960 B) | **6,248 B** | **5,128 B** | **1,896 B** | **+0 B** | **PASS** |

### 5.3 MCU Baseline Reconciliation

[`docs/MCU_BASELINE.md`](../../../docs/MCU_BASELINE.md) was updated to document these verified numbers and clarify the boundary between host simulator measurements and final target firmware RAM:
- Host static draw buffer: 25,600 B ($800 \times 16 \times 2\text{ B}$).
- LVGL internal heap: 40,960 B (40 KiB constrained) / 43,008 B (42 KiB default).
- UI static descriptors & history: 1,328 B (including 64-sample 32-bit history storage).
- **Target Firmware Disclaimer:** Final MCU firmware memory map (FreeRTOS task stacks, DMA buffers, radio packet queues, Modbus buffers) must be confirmed on target hardware.

---

## 6. Carried E2 Combined Fault Matrix

Implemented **CHECK 40** in [`sim_pc/main.c`](../../../sim_pc/main.c), asserting a 6-step table-driven sequence:

```text
[CHECK 40] Table-driven combined fault matrix and recovery verification...
  Step 0: Dual simultaneous fault (CO2 offline timeout + VOC 0xFFFF invalid) asserted.
          Verified Home banner, live trends status, Auto editor pause labels, and unaffected metrics.
  Step 1: 1-minute autonomous history advance with active faults.
          Verified invalid gap slot recorded for CO2 and VOC; Temp and Humid history intact.
  Step 2: Partial recovery (CO2 recovers to 1100 ppm, VOC remains invalid).
          Verified 10s anti-short-cycle hold suppresses relay activation.
  Step 3: Post-hold 3-frame qualification.
          Verified Fan turns ON, active_low wire GPIO bit 1 == 0, node echo/readback verified.
  Step 4: Valid zero baseline reading (VOC = 0 index).
          Verified Purifier evaluates valid zero reading without fault flag.
  Step 5: Full recovery to normal operation.
          Verified clean banner, all valid flags, zero active faults.
CHECK 40 PASSED.
```

### Table of Asserted Fault & Recovery States

| Step | State Description | CO₂ Status | VOC Status | Temp / Humid | Fan Relay Wire | Purifier Relay Wire | History Impact |
| :---: | :--- | :---: | :---: | :---: | :---: | :---: | :--- |
| **0** | **Dual Fault** | Timed out (`valid=false`) | `0xFFFF` (`valid=false`) | Valid ($23.5^\circ\text{C}$, $46\%$) | Safe Off (GPIO 1) | Safe Off (GPIO 1) | Gaps flagged; Temp/Humid valid |
| **1** | **Gap Slot** | Gap logged (`valid=false`) | Gap logged (`valid=false`) | Valid logged | Safe Off (GPIO 1) | Safe Off (GPIO 1) | Autonomous minute slot recorded |
| **2** | **Partial Recovery** | $1100\text{ ppm}$ (`valid=true`) | `0xFFFF` (`valid=false`) | Valid | Held Off (10s hold) | Safe Off (GPIO 1) | 10s anti-short-cycle hold active |
| **3** | **Qualified Actuation** | $1100\text{ ppm}$ (3 frames) | `0xFFFF` (`valid=false`) | Valid | **Active ON (GPIO 0)** | Safe Off (GPIO 1) | `active_low` wire polarity asserted |
| **4** | **Valid Zero Baseline** | $800\text{ ppm}$ | $0\text{ index}$ (`valid=true`) | Valid | Auto Off (GPIO 1) | Auto Off (GPIO 1) | Zero is valid, not error |
| **5** | **Full Recovery** | $600\text{ ppm}$ | $80\text{ index}$ | Valid | Normal Auto | Normal Auto | All faults cleared, clean UI |

---

## 7. Delivery Artifacts & Cryptographic Signatures

### 7.1 SHA-256 Signatures

| File | SHA-256 Hash |
| :--- | :--- |
| [`ui.c`](../../../ui.c) | `02e35ce3db51938cd6a74babd8411e9cb39d7ad9f7ab4b995e46e4e3c85112cd` |
| [`ui.h`](../../../ui.h) | `d1130a23484a2e00462755bcd5b7a007c958cb1a3b1a51392e918cf00574b6e2` |
| [`ui_types.h`](../../../ui_types.h) | `5a3952d632fdfb852bfaf4da761e4f7fa3074bea762df3a2608be462d4f24715` |
| [`sim_pc/ui_test_hooks.h`](../../../sim_pc/ui_test_hooks.h) | `7536ef6d59225bf0bf42145a45dc14fd433735c4acb3ea9b3ce6f5c3a54e6230` |
| [`sim_pc/CMakeLists.txt`](../../../sim_pc/CMakeLists.txt) | `be3e7c84daa1c77178be4c3df79a9241a1f62ca2f6563006c30d55f910afacec` |
| [`sim_pc/main.c`](../../../sim_pc/main.c) | `37a531791aea71b1e537780707aa813291ccecd818f050cdc4c4efd59bbcf88e` |
| [`tools/verify_production.ps1`](../../../tools/verify_production.ps1) | `3f60997b0586d9434d4e25de8832b6c837cc546b4c88a82cb26fa3e94eab2ed5` |
| [`docs/MCU_BASELINE.md`](../../../docs/MCU_BASELINE.md) | `9c5a7bc7eacc1a91c46a04b122d3fdae43c34edd4571dbf942a93389c3e855b8` |
| [`sim_pc/build/sim_pc.exe`](../../../sim_pc/build/sim_pc.exe) | `5bfd148acd92b7bbceed203ac44a5346c1ae0bf2db0b3b8226476813d9304cff` |
| [`build-40k/sim_pc.exe`](../../../build-40k/sim_pc.exe) | `b893268946f275b765d59b9e7151d9bf150f5f18bc2919f5c5c1930d7054df08` |
| [`benchmark_fixture.c`](./benchmark_fixture.c) | `9a0b7ef5d917336a1e16d2a7be5db8a24de13baf7be12f6b27864f1f7ed0d1b7` |
| [`benchmark_fixture.exe`](./benchmark_fixture.exe) | `c4de14b8123f08770ecbb2deaa7ec1744ca7d29616005a9ebfd43335a3bfcbd5` |

### 7.2 Captured Visual Evidence

Key UI scenarios captured during regression testing are saved in [`captures/`](./captures/):
- **Trends Screens:** [`trends_data.png`](./captures/trends_data.png) (CO₂ populated), [`trends_voc.png`](./captures/trends_voc.png), [`trends_temp.png`](./captures/trends_temp.png), [`trends_humid.png`](./captures/trends_humid.png), [`trends_empty.png`](./captures/trends_empty.png).
- **Home States:** [`home_good.png`](./captures/home_good.png), [`home_sensor_fail.png`](./captures/home_sensor_fail.png), [`home_offline.png`](./captures/home_offline.png), [`home_auto_paused.png`](./captures/home_auto_paused.png).
- **Devices & Auto:** [`devices_grid.png`](./captures/devices_grid.png), [`devices_settings.png`](./captures/devices_settings.png), [`devices_auto_thresholds.png`](./captures/devices_auto_thresholds.png), [`devices_auto_purifier.png`](./captures/devices_auto_purifier.png), [`devices_auto_humidifier.png`](./captures/devices_auto_humidifier.png).

---

## 8. Conclusion

All findings from the independent review and requirements in `CURRENT.md` have been fulfilled and verified:
- Two-stage tactile response and burst-tap coalescing eliminate the host rendering bottleneck.
- Process cleanup fixtures are verified safe and isolated.
- Allocation-level heap tracing confirms zero failures and healthy headroom down to 40 KiB.
- The combined fault matrix is durably asserted in regression check 40.
- Hardware latency on target LCD is formally declared UNVERIFIED pending owner's oscilloscope traces.
