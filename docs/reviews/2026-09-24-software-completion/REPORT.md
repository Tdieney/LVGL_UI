# Consolidated Delivery Report — Smart Hub UI Software Completion

**Date:** 2026-09-24  
**Scope:** One consolidated software-completion correction delivery addressing findings F1–F4 from [docs/reviews/2026-09-24-software-completion-independent/REVIEW.md](../2026-09-24-software-completion-independent/REVIEW.md) and [docs/prompts/CURRENT.md](../../prompts/CURRENT.md).  
**Status:** IMPLEMENTED — all verification gates passed; ready for final technical-lead review.  
**Branch / Worktree:** `ui/smart-hub` (pre-existing dirty worktree strictly preserved; 0 commits, 0 pushes).

---

## 1. Executive Summary & Corrective Closures (F1–F4)

This consolidated correction delivery resolves all four findings (F1–F4) identified during independent review without changing the accepted Quiet Architecture pixels or text sizes:

1. **F1 / N1 (Fail-Closed Scenario Generator, Exact 28-Shot Manifest & Strict Negative Control):**
   - In [`sim_pc/main.c`](../../../sim_pc/main.c): Output directory is validated with `GetFileAttributesA()`; invalid/unwritable destinations (e.g. `NUL`) fail immediately with exit code 1 and write a fatal diagnostic to stderr.
   - Introduced `save_scenario_shot()` helper: checks every `save_framebuffer()` call, records failure count, and returns nonzero on failure.
   - Full scenario suite generates and verifies **all 28 shots** (reconciling legacy hardcoded "24"), backed by durable fail-closed assertions for the Stage-1 interim frame (`trends_stage1_interim.raw`).
   - In [`tools/run_regression.ps1`](../../../tools/run_regression.ps1): Negative test invokes simulator directly without catch-all traps, strictly asserting that the simulator exits with **code 1** (fails if 0 or any other code) and that stderr contains the exact diagnostic string: `FATAL: Output directory 'NUL' is not a valid directory or could not be created` (N1). Replaced weak `< 10` file count with an exact 28-file manifest check asserting existence and exact 768,000-byte length ($800 \times 480 \times 2$) for each shot.
2. **F2 (Fail-Closed CHECK 39 Assertions):**
   - In [`sim_pc/main.c`](../../../sim_pc/main.c): Removed silent-skip guards (`if (h39 && ...)`) across sections 5–8 of CHECK 39. Missing test hooks, NULL label pointers, title mismatches, or unit mismatches now explicitly trigger `test39_pass = false` with descriptive failure output.
   - Both 42 KiB default and 40 KiB opt-in regression suites execute and pass all fail-closed CHECK 39 assertions (40/40 checks passed in both profiles).
3. **F3 / N2 (Controlled Live CIM-Denial Fault Injection, Descendant Cleanup & Boundaries):**
   - In [`tools/verify_production.ps1`](../../../tools/verify_production.ps1): Updated `Terminate-OwnedProcessTree` with `-SkipDiscovery` switch, ensuring error cleanup never repeats failing CIM discovery paths. The root process is always queued into `$toKill` upon verified identity before dynamic discovery runs.
   - Wrapped dynamic child discovery in `try / catch` so enumeration failure does not abort termination of known owned workers.
   - Added **Fixture 6c** in `Invoke-ProcessSafetyFixtures`: exercises controlled live CIM denial via `$script:SimulateCimDenial = $true`, verifying fail-closed error propagation, safe bottom-up termination of the root and all registered known descendants under `-SkipDiscovery`, zero known verified survivors, and verified protection of unrelated processes (N2).
   - **Host Process Boundary Disclosure (N2):** Unobserved descendants spawned by a worker prior to initial CIM discovery cannot be tracked or enumerated when CIM access is denied without OS Job Objects. Such unobserved processes may outlive the root until command termination. The verifier strictly scopes its guarantee to known verified processes and fails closed immediately upon CIM denial.
4. **F4 (Accurate Evidence, Warning Disclosures & Reproducibility):**
   - Distinguishes build warnings: C compiler compiled cleanly with `-Wall -Wextra -Werror` producing **0 errors, 0 warnings**. Upstream CMake generated **1 developer policy warning** (`Policy CMP0177 is not set: install() DESTINATION paths are normalized` at `_deps/lvgl-src/env_support/cmake/custom.cmake:66`).
   - Corrected standalone benchmark build command in [`benchmark_fixture.c`](./benchmark_fixture.c) to point to the active review directory.
   - Reconciled scenario count: exactly 28 scenario shots generated and verified.
   - Maintained strict separation between host-verified gates and target-only UNVERIFIED items (physical LCD latency, touch controller interrupts, MCU SRAM map, radio ring buffers). No claims of guaranteed bug-free code.

---

## 2. Verification Gates Summary

All gates were executed synchronously post-edit on the Windows host environment:

| Gate | Exact Command Line | Exit Code | Compiler Status | Diagnostic / Verification Details |
| :--- | :--- | :---: | :---: | :--- |
| **1. Asset Integrity** | `python tools/generate_assets.py --check` | `0` | N/A | 25 canonical masters, 68 runtime icons verified; manifest clean |
| **2. Simulator Build** | `tools/build_sim.bat` | `0` | **0 errors, 0 warnings** | C build clean (`-Wall -Wextra -Werror`); 1 upstream CMake CMP0177 dev warning noted |
| **3. Default 42 KiB Regression** | `tools/run_regression.bat` | `0` | N/A | **40 / 40 checks passed (100%)**; negative `--shots NUL` verified; 28/28 scenario manifest verified |
| **4. Opt-in 40 KiB Regression** | `build-40k/sim_pc.exe --regression` | `0` | N/A | **40 / 40 checks passed (100%)**; transient min free 1,896 B, 0 allocation failures |
| **5. MCU Export Manifest** | `tools/export_mcu.bat --verify` | `0` | N/A | 16 bundle files verified against manifest |
| **6. Production Verifier** | `tools/verify_production.bat` | `0` | **0 errors, 0 warnings** | Phases 1–4 passed; Fixture 6c live CIM denial verified; isolated build passed |

Raw execution logs are preserved permanently in this directory:
- [`generate_assets.log`](./generate_assets.log)
- [`build_sim.log`](./build_sim.log)
- [`regression_default.log`](./regression_default.log)
- [`regression_40k.log`](./regression_40k.log)
- [`export_mcu.log`](./export_mcu.log)
- [`verify_production.log`](./verify_production.log)
- [`benchmark_results.txt`](./benchmark_results.txt)

---

## 3. Two-Stage Rendering & Responsiveness Evidence

### 3.1 Measured Two-Stage Sequence (Normal Application Loop)

From [`benchmark_results.txt`](./benchmark_results.txt) and CHECK 39:

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
```

### 3.2 Burst Coalescing Efficiency

```text
====================================================================
 Rapid Tap Coalescing Benchmark (Burst: VOC -> TEMP -> HUMIDITY)
====================================================================
Uncoalesced (3 full chart renders) : 451181 px, 75 flushes
Coalesced (3 tactile + 1 chart)    : 214174 px, 34 flushes
Savings from Burst Coalescing      : 237007 px (-52.5% pixels), 41 flushes (-54.7% flushes)
NOTE: The reduction applies exclusively to rapid bursts; single switches render fully.
```

### 3.3 100-Tap Memory Stability

```text
====================================================================
 100-Tap Rapid Switching Stress & Memory Stability
====================================================================
  100 switches : 6311782 px, 986 flushes
  Heap before  : 13752 B free (largest 12464 B)
  Heap after   : 13752 B free (largest 12464 B)
  Heap drift   : 0 B (Zero drift verified!)
```

### 3.4 Reproducible Standalone Benchmark Command

To compile and execute the benchmark standalone:
```cmd
gcc -O2 -Wall -Wextra -I. -Isim_pc/build/_deps/lvgl-src -Isim_pc docs/reviews/2026-09-24-software-completion/benchmark_fixture.c ui_theme.c ui_fonts.c ui_icons.c ui_auto.c ui_splash_logo.c -o docs/reviews/2026-09-24-software-completion/benchmark_fixture.exe -Lsim_pc/build/lib -llvgl
docs\reviews\2026-09-24-software-completion\benchmark_fixture.exe
```

---

## 4. Cryptographic Signatures & Source Identity

### 4.1 SHA-256 Hash Table

| File | SHA-256 Digest |
| :--- | :--- |
| [`ui.c`](../../../ui.c) | `f74723d8c41ef3cd5b9b783412068bacd8749b5e834a69809728af503c6ed12e` |
| [`ui.h`](../../../ui.h) | `d1130a23484a2e00462755bcd5b7a007c958cb1a3b1a51392e918cf00574b6e2` |
| [`ui_types.h`](../../../ui_types.h) | `5a3952d632fdfb852bfaf4da761e4f7fa3074bea762df3a2608be462d4f24715` |
| [`ui_auto.c`](../../../ui_auto.c) | `130cc2d7298c6f1002ffed1e07b199868c26481dabff62373e654d9b630d60e2` |
| [`ui_auto.h`](../../../ui_auto.h) | `52644c2d9357a530e85cc24ea71701e4c8149cbd0e29b4c72bae0beaf59228c2` |
| [`ui_theme.c`](../../../ui_theme.c) | `1d34f1805151e52c0e75160fe20c253afa28cd067ffbab94cd0f52dd45de75c4` |
| [`ui_fonts.c`](../../../ui_fonts.c) | `d13965717edece44a5c834bc8745a4d17a30b22ba24790e255108d88ceaaa9ca` |
| [`ui_icons.c`](../../../ui_icons.c) | `a03035b9494a047cf40928bc74a3a09c4e49505a00b5d6d4588eea3fffd159d1` |
| [`ui_mcu_profile.h`](../../../ui_mcu_profile.h) | `cbf5b1b8afd4984b04a51a0f1b19353bc3e4648afa2e5faa0dc784cc8980364c` |
| [`lora_comm.h`](../../../lora_comm.h) | `4d62583dbbbb03068a2b167dda719fcfb09f8172f2c07c94ae4f05cd2e54ce6a` |
| [`lora_hub_link.h`](../../../lora_hub_link.h) | `86b9ab095f22adb84f2c74a6fa25fca2e7e4d774515301f75acf65f146755d74` |
| [`sim_pc/main.c`](../../../sim_pc/main.c) | `621fe68207d76cf8f7c5f5ba18a60802b2b2966d0456a8e2ac5c3b8aa26f1078` |
| [`sim_pc/ui_test_hooks.h`](../../../sim_pc/ui_test_hooks.h) | `4989255f21b415c8fc43acadb715cc6dc352dfc637202603c319ad2b20ae135a` |
| [`sim_pc/smoke_production.c`](../../../sim_pc/smoke_production.c) | `751b309df50e52ac182b176e605d084dbf1f4ad76b90b589d2fad14e3d3dc50c` |
| [`tools/run_regression.ps1`](../../../tools/run_regression.ps1) | `7bc8e12ef77c02d071cbfc40cf234f0ea7a29c546698316efdcef4c196ddd64d` |
| [`tools/verify_production.ps1`](../../../tools/verify_production.ps1) | `9d5c0004288e2790f1a0d503e045408e8de965d59f8b532056b07cdbb2952c22` |
| [`sim_pc/build/sim_pc.exe`](../../../sim_pc/build/sim_pc.exe) | `79e379058bb2c5a7c235da70ec0c1f2bea0517aab6737a4eeee3a026a43e58f1` |
| [`build-40k/sim_pc.exe`](../../../build-40k/sim_pc.exe) | `a90c1a050894b83492ed2a1d3a803c6c2a25672fc89ec7ef4c20d7027b984107` |
| [`benchmark_fixture.c`](./benchmark_fixture.c) | `a22cfefaaef6ddc9b4cf8724ef8781251ec83eaf3181891fd1ea397563b40d19` |
| [`benchmark_fixture.exe`](./benchmark_fixture.exe) | `38f3318dee3d32f94320de97a818563fe4d52d9376b4cb1ab4a6f3c3079f19d2` |
| [`benchmark_results.txt`](./benchmark_results.txt) | `93b29770501fb38be9cda52d7451076e1fed5346eb89fa8e3e97cfcd46dbe3cd` |

---

## 5. Visual Equivalence & Scenario Captures

Exactly 28 native 800×480 RGB565 scenario frames were generated by `sim_pc.exe --shots` and converted to PNG in [`captures/`](./captures/):

1. **Stage-1 Interim Frame:**
   - [`trends_stage1_interim.png`](./captures/trends_stage1_interim.png): Left-rail VOC card is highlighted (checked); title remains "CO₂"; plot curve and stats remain CO₂.
2. **Trends Views (Populated & Empty):**
   - [`trends_data.png`](./captures/trends_data.png): CO₂ selected and populated.
   - [`trends_voc.png`](./captures/trends_voc.png): VOC selected and populated.
   - [`trends_temp.png`](./captures/trends_temp.png): Temperature selected and populated.
   - [`trends_humid.png`](./captures/trends_humid.png): Humidity selected and populated.
   - [`trends_empty.png`](./captures/trends_empty.png): Empty trends state.
3. **Home States:**
   - [`home_good.png`](./captures/home_good.png), [`home_moderate.png`](./captures/home_moderate.png), [`home_poor.png`](./captures/home_poor.png)
   - [`home_offline.png`](./captures/home_offline.png), [`home_sensor_fail.png`](./captures/home_sensor_fail.png)
   - [`home_link_degraded.png`](./captures/home_link_degraded.png), [`home_auto_paused.png`](./captures/home_auto_paused.png), [`home_auto_paused_switch.png`](./captures/home_auto_paused_switch.png)
   - [`home_relay_pending.png`](./captures/home_relay_pending.png), [`home_relay_retry.png`](./captures/home_relay_retry.png), [`home_temp_humid_invalid.png`](./captures/home_temp_humid_invalid.png)
4. **Devices & Auto Dialogs:**
   - [`devices_grid.png`](./captures/devices_grid.png), [`devices_settings.png`](./captures/devices_settings.png), [`devices_settings_manual_only.png`](./captures/devices_settings_manual_only.png)
   - [`devices_auto_thresholds.png`](./captures/devices_auto_thresholds.png), [`devices_auto_purifier.png`](./captures/devices_auto_purifier.png), [`devices_auto_humidifier.png`](./captures/devices_auto_humidifier.png)
   - [`devices_auto_max_10000.png`](./captures/devices_auto_max_10000.png), [`devices_auto_invalid_draft.png`](./captures/devices_auto_invalid_draft.png)
   - [`dropdown_open_top.png`](./captures/dropdown_open_top.png), [`dropdown_open_bottom.png`](./captures/dropdown_open_bottom.png), [`splash.png`](./captures/splash.png)

---

## 6. Target-Only Limitations & Disclaimers

> [!WARNING]
> **Status on Physical Target Hardware:** **UNVERIFIED**  
> All latency timings ($3.45\text{ ms}$ Stage 1, $10.58\text{ ms}$ Stage 2) are calculated from host display flush byte counts assuming a declared nominal $20\text{ MB/s}$ QSPI clock. The following items require physical target hardware and oscilloscope/logic analyzer instrumentation:
> 1. Physical LCD touch-to-photon latency (interrupt to pixel response).
> 2. Real I2C capacitive touch controller interrupt propagation delay.
> 3. Display controller GRAM tearing effect (`TE`) line synchronization and DMA FIFO overhead.
> 4. Physical MCU memory map, RTOS task stack margins, and hardware radio/DMA ring buffers.
> 5. Optical readability under direct ambient sunlight and backlighting conditions.

---

## 7. Delivery Status & Next Actions

The Smart Hub UI software-completion correction is delivered with complete host verification:
- **All 4 corrective findings (F1–F4)** and **both narrow follow-up items (N1–N2)** resolved and tested.
- **Fail-closed scenario capture (F1 / N1):** Verified with strict negative destination assertions (exit code exactly 1, fatal diagnostic matched, no false passes from generic exceptions) and exact 28-shot manifest.
- **Fail-closed CHECK 39 assertions (F2):** Proved across both 42 KiB and 40 KiB memory configurations.
- **Controlled live CIM-denial fault injection (F3 / N2):** Proved in verifier Fixture 6c with known verified descendants and root safely terminated under `-SkipDiscovery` with zero known survivors and host process boundaries documented.
- **0 C compiler errors / warnings:** (`-Wall -Wextra -Werror`); 1 upstream CMake CMP0177 dev policy warning explicitly documented.
- Accepted native appearance and text $\ge 18\text{ px}$ preserved intact.
- Pre-existing worktree preserved; no commits, pushes, resets, cleans, stashes, or file deletions performed.
