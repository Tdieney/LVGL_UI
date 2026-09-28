# Smart Hub UI — Verification Completion Evidence Report

**Date:** 2026-09-17  
**Scope:** Consolidated delivery of requirements R1–R4 from [CURRENT.md](file:///c:/Users/vanth/Documents/MyDocument/Embedded/LVGL_UI/docs/prompts/CURRENT.md) addressing the findings of the 2026-09-17 independent review.  
**Status:** ALL 31 CHECKS PASS (Exit Code 0). Production Verification Suite PASS (Phases 1–4, Exit Code 0). Manifest Verification PASS (Exit Code 0).

---

## 1. Executive Summary & Verification Matrix

All requirements (R1–R4) have been implemented and verified in a single consolidated delivery:

| ID | Focus Area | Requirement & Resolution | Automated Tests | Status |
|---|---|---|---|---|
| **R1** | 50-Cycle Stress Lifecycle & Headroom | Upgraded CHECK 31 to execute genuine Auto/recovery transitions across 50 cycles (pending, timeout, error latch + dropdown + toast, recovery adopting wire mask, explicit re-arm). Recorded state counters (`cmds=10, pend=5, timeout=5, latch=5, recov=5, rearm=5`). Verified >4096 B free memory across all states. | `sim_pc --regression` (CHECK 31) | **PASS** (Immediate Min: 5344 B, Settled Min: 4496 B, Drift: -56 B) |
| **R2** | Finite Time & Touch Matrix | 1) Extended `UINT32_MAX` rollover to test all 3 Auto rules in both directions (Fan, Purifier, Humidifier) with 3-frame qualification; 2) Tested link age 4999 ms vs 5000 ms boundary driving `ui_tick()` and wire TX refusal; 3) Executed BOTH Switch and Retry actions across Home and Devices tabs from re-established setups. | `sim_pc --regression` (CHECK 1, CHECK 28) | **PASS** (All 6 rule transitions verified across wrap; link stale path verified; both actions executed) |
| **R3** | Export Verification Hardening | Upgraded `tools/verify_production.ps1` with: 1) Test 7 inspecting BOTH concurrent default ZIPs (exact paths, distinct files, archive membership, source identity, sentinel preservation); 2) Test 8 explicit destination race; 3) Test 9 controlled failure & watchdog process-tree reaping (`taskkill /PID /T /F`); 4) Canonical temp path boundary checks. | `tools/verify_production.bat` (Phase 1) | **PASS** (16 verified files, race safety, controlled failure & watchdog verified) |
| **R4** | Truthful Evidence & Documentation | 1) Replaced unmeasured `< 50 ns` claim in `sim_pc/main.c` with `(nominal check, 0 flushes)`; 2) Captured all 24 scenario framebuffers as PNGs in `shots/`; 3) Captured full unabridged logs; 4) Updated repository tracking documentation and `CURRENT.md` status header. | `build_sim.log`, `regression.log`, `export_verify.log`, `verify_production.log` | **PASS** (Honest metrics, 24 shots generated, zero truncation) |

---

## 2. Requirement R1: 50-Cycle Stress Lifecycle & Heap Headroom

### Lifecycle Transitions & State Counters
In [sim_pc/main.c](file:///c:/Users/vanth/Documents/MyDocument/Embedded/LVGL_UI/sim_pc/main.c), CHECK 31 configures Device 0 (Air Purifier) with Auto thresholds (ON=150, OFF=100) and advances through 50 full cycles of navigation, settings dialog invocation, open dropdown inspection, genuine invalid draft validation, and deliberate Auto lifecycle events:
- **Pending:** Auto triggers outbound command when VOC $\ge 150$ ppm over 3 qualified frames.
- **Timeout:** 3000 ms timeout elapses without node ACK, transitioning command phase to `UI_CMD_PHASE_ERROR`.
- **Error Latching:** Paused state is latched, verified on Home tile mode/state banner, open dropdown list, and active toast message.
- **Recovery:** Wire state reports expected relay mask and matching sequence number, clearing error phase.
- **Explicit Re-arm:** Re-armed via simulated action or auto threshold re-evaluation.

**Verified State Counters (50 Cycles):**
- Commands Sent: **10**
- Pending States: **5**
- Timeouts: **5**
- Error Latches: **5**
- Recoveries: **5**
- Explicit Rearms: **5**

### Memory Headroom & Fragmentation Metrics
Measurements were tracked in both immediate and settled states to guarantee no transient allocation drops below the MCU floor (4096 bytes free out of 42 KiB total heap):

| Metric | Immediate Measurement | Settled/Stable Measurement | Threshold | Status |
|---|---|---|---|---|
| **Peak Used** | 88% | 90% | $< 95\%$ | **PASS** |
| **Min Total Free** | **5,344 bytes** (Device settings first rendered) | **4,496 bytes** (Auto limits open) | $> 4,096\text{ bytes}$ | **PASS** |
| **Min Largest Block** | 5,104 bytes | 4,064 bytes | $> 2,048\text{ bytes}$ | **PASS** |
| **Peak Fragmentation** | 47% | 62% | $< 75\%$ | **PASS** |
| **Stable Drift (Cycles 10 $\to$ 50)** | — | **-56 bytes** | $\le 256\text{ bytes}$ | **PASS** |

---

## 3. Requirement R2: Finite Time and Touch Matrix

### 3.1 32-Bit Millisecond Rollover (`UINT32_MAX`) Matrix
In [sim_pc/main.c](file:///c:/Users/vanth/Documents/MyDocument/Embedded/LVGL_UI/sim_pc/main.c) CHECK 1, rollover was expanded to comprehensively exercise all three Auto rules in both turn-on and turn-off directions across `UINT32_MAX` boundary, asserting that the 10-second hold timer and 3-frame qualification logic use unsigned subtraction `(uint32_t)(now - start)`:

1. **Rule 1 — Ventilation Fan / $\text{CO}_2$ (Device 1):**
   - Turn-On ($\text{CO}_2 \ge 1000\text{ ppm}$): Frame 1 & 2 return `false`, Frame 3 returns `true` (desired ON).
   - Turn-Off ($\text{CO}_2 \le 800\text{ ppm}$): Frame 1 & 2 return `false`, Frame 3 returns `true` (desired OFF).
2. **Rule 2 — Air Purifier / VOC (Device 0):**
   - Turn-On ($\text{VOC} \ge 150\text{ ppb}$): Frame 1 & 2 return `false`, Frame 3 returns `true` (desired ON).
   - Turn-Off ($\text{VOC} \le 100\text{ ppb}$): Frame 1 & 2 return `false`, Frame 3 returns `true` (desired OFF).
3. **Rule 3 — Humidifier / RH (Device 2):**
   - Turn-On ($\text{RH} \le 40\%$): Frame 1 & 2 return `false`, Frame 3 returns `true` (desired ON).
   - Turn-Off ($\text{RH} \ge 50\%$): Frame 1 & 2 return `false`, Frame 3 returns `true` (desired OFF).
4. **Boot Tick 0:** Verified that at tick 0 the hold timer and qualification operate normally without division-by-zero or signed underflow.

### 3.2 Link Staleness 4999 ms vs. 5000 ms Execution Path
In [sim_pc/main.c](file:///c:/Users/vanth/Documents/MyDocument/Embedded/LVGL_UI/sim_pc/main.c) CHECK 1, realistic link staleness was tested by advancing LVGL ticks using exact increments to hit exact millisecond boundaries:
- **At 4999 ms (`now - lora_last_rx_tick_ms == 4999`):**
  - Header displays `Connected` (1 bar minimum floor).
  - User toggle switch input is accepted.
  - UI emits relay command packet on wire.
- **At 5000 ms (`now - lora_last_rx_tick_ms == 5000`):**
  - Header transitions to `Disconnected`.
  - User toggle input is rejected (switch does not flip, command queue remains empty).
  - Auto decision qualification refuses to trigger decisions during disconnected state.
  - Periodic TX loop is suppressed (0 outbound packets emitted).
- **Recovery:** Upon receiving fresh frame with updated revision, connection status immediately restores to `Connected` and readback mask is adopted.

### 3.3 Touch Action Matrix: Both Switch and Retry Executed
In [sim_pc/main.c](file:///c:/Users/vanth/Documents/MyDocument/Embedded/LVGL_UI/sim_pc/main.c) CHECK 28:
- **Action 1 (Switch):** Clicked at recovered switch hitbox $(x=214)$. Verified:
  - Mode switches immediately from Auto to Manual (`UI_MODE_MANUAL`).
  - Latch is cleared.
  - Exactly 1 command packet is emitted with new relay mask.
  - Other channels remain completely unaffected.
- **Action 2 (Retry):** Setup re-established via `ui_test_reset_device_commands()` and `advance_ui(50u)`. Clicked at Retry button hitbox $(x=284)$. Verified:
  - Device remains in Auto mode (`UI_MODE_AUTO`).
  - Error latch is cleared and hold timer is restarted.
  - No stale or obsolete manual command is emitted.
- Verified across both Home and Devices tabs.

---

## 4. Requirement R3: Export Verification & Script Hardening

The verification script [tools/verify_production.ps1](file:///c:/Users/vanth/Documents/MyDocument/Embedded/LVGL_UI/tools/verify_production.ps1) was systematically hardened to eliminate transient hangs, race vulnerabilities, and weak assertions:

### 4.1 Concurrent Default ZIP Inspection (Test 7)
- Launched two concurrent export processes targeting the default output directory.
- Verified both processes exited with code 0.
- **Inspected BOTH resulting ZIP archives:**
  - Both archives exist and paths are distinct (`smart_hub_ui_YYYYMMDD_HHmm.zip` and `smart_hub_ui_YYYYMMDD_HHmm_1.zip`).
  - Both archives contain all 16 required export files (`ui.c`, `ui.h`, `ui_types.h`, `ui_theme.c`, `ui_theme.h`, `ui_mcu_profile.h`, `lora_comm.h`, 9 font files).
  - Content of files inside both ZIPs was extracted and verified byte-for-byte identical to the workspace source files.
  - Pre-existing files in the export directory (sentinels) were preserved intact.

### 4.2 Explicit Destination Directory Race (Test 8)
- Launched two concurrent export processes competing for the *exact same* explicit destination directory `--dest`.
- Verified race safety: exactly one process wins (exit code 0), and exactly one process loses (exit code 1 reporting that destination directory already exists).
- Verified the winning archive/directory was not corrupted and contains all 16 verified files.

### 4.3 Controlled Failure Paths & Watchdog Reaping (Test 9)
- **Controlled Failure:** Executed export from a corrupted staging tree where `ui.c` was deleted. Verified export failed with nonzero exit code, printed a clear diagnostic identifying missing `ui.c`, and left pre-existing sentinel files untouched.
- **Process Watchdog & Process-Tree Reaping:** Implemented `Invoke-ConcurrentWorkers` managing background processes in `try/finally` blocks with asynchronous stream reading. Used `taskkill.exe /PID $pid /T /F` to ensure that timed-out processes and all child/grandchild subprocesses are forcibly terminated.

### 4.4 Temp Directory Cleanup Safety
- Fixture directories validate that the target directory resides strictly within `$env:TEMP` before invoking recursive deletion (`Remove-Item -Recurse -Force`), guarding against accidental deletion of root or workspace paths.

---

## 5. Requirement R4: Truthful Evidence & Artifacts

### 5.1 Honest Performance Flush Assertion
In [sim_pc/main.c](file:///c:/Users/vanth/Documents/MyDocument/Embedded/LVGL_UI/sim_pc/main.c) line 4566, the unmeasured `< 50 ns` output was updated:
```c
// Before:
// printf("  (f) Per-tick Change Test:           < 50 ns (revision check, 0 flushes)\n");
// After:
printf("  (f) Per-tick Change Test:           Revision counter comparison + link staleness check (nominal check, 0 flushes)\n");
```

### 5.2 Verification Log Artifacts
All verification commands were executed and complete, unabridged logs saved to this directory:
- [build_sim.log](file:///c:/Users/vanth/Documents/MyDocument/Embedded/LVGL_UI/docs/reviews/2026-09-17-verification-completion/build_sim.log): Simulator build log (Release, Ninja).
- [regression.log](file:///c:/Users/vanth/Documents/MyDocument/Embedded/LVGL_UI/docs/reviews/2026-09-17-verification-completion/regression.log): Full 31/31 regression suite execution log, including heap breakdown, drift, and performance metrics.
- [export_verify.log](file:///c:/Users/vanth/Documents/MyDocument/Embedded/LVGL_UI/docs/reviews/2026-09-17-verification-completion/export_verify.log): Manifest verification log (`tools/export_mcu.bat --verify`).
- [verify_production.log](file:///c:/Users/vanth/Documents/MyDocument/Embedded/LVGL_UI/docs/reviews/2026-09-17-verification-completion/verify_production.log): Full 4-phase production verification run (manifest, export race/watchdog tests, isolated CMake build, smoke test, negative control).

### 5.3 Deterministic Scenario Screenshots (24 Framebuffers)
All 24 scenario framebuffers were rendered by `sim_pc.exe --shots` and converted to PNG:
1. [splash.png](file:///c:/Users/vanth/Documents/MyDocument/Embedded/LVGL_UI/docs/reviews/2026-09-17-verification-completion/shots/splash.png): Splash boot screen.
2. [home_good.png](file:///c:/Users/vanth/Documents/MyDocument/Embedded/LVGL_UI/docs/reviews/2026-09-17-verification-completion/shots/home_good.png): Home view with optimal telemetry.
3. [home_moderate.png](file:///c:/Users/vanth/Documents/MyDocument/Embedded/LVGL_UI/docs/reviews/2026-09-17-verification-completion/shots/home_moderate.png): Home view with moderate telemetry.
4. [home_poor.png](file:///c:/Users/vanth/Documents/MyDocument/Embedded/LVGL_UI/docs/reviews/2026-09-17-verification-completion/shots/home_poor.png): Home view with poor telemetry.
5. [home_offline.png](file:///c:/Users/vanth/Documents/MyDocument/Embedded/LVGL_UI/docs/reviews/2026-09-17-verification-completion/shots/home_offline.png): Home view with offline sensors.
6. [home_auto_paused_switch.png](file:///c:/Users/vanth/Documents/MyDocument/Embedded/LVGL_UI/docs/reviews/2026-09-17-verification-completion/shots/home_auto_paused_switch.png): Home view showing paused Auto state with interactive switch.
7. [home_sensor_fail.png](file:///c:/Users/vanth/Documents/MyDocument/Embedded/LVGL_UI/docs/reviews/2026-09-17-verification-completion/shots/home_sensor_fail.png): Home view showing sensor failure state.
8. [home_relay_pending.png](file:///c:/Users/vanth/Documents/MyDocument/Embedded/LVGL_UI/docs/reviews/2026-09-17-verification-completion/shots/home_relay_pending.png): Home view showing relay command pending.
9. [home_relay_retry.png](file:///c:/Users/vanth/Documents/MyDocument/Embedded/LVGL_UI/docs/reviews/2026-09-17-verification-completion/shots/home_relay_retry.png): Home view showing relay retry prompt.
10. [trends_data.png](file:///c:/Users/vanth/Documents/MyDocument/Embedded/LVGL_UI/docs/reviews/2026-09-17-verification-completion/shots/trends_data.png): Trends view populated with 24-hour telemetry.
11. [trends_empty.png](file:///c:/Users/vanth/Documents/MyDocument/Embedded/LVGL_UI/docs/reviews/2026-09-17-verification-completion/shots/trends_empty.png): Trends view in empty state.
12. [devices_grid.png](file:///c:/Users/vanth/Documents/MyDocument/Embedded/LVGL_UI/docs/reviews/2026-09-17-verification-completion/shots/devices_grid.png): Devices grid view showing 4 appliance cards.
13. [devices_settings.png](file:///c:/Users/vanth/Documents/MyDocument/Embedded/LVGL_UI/docs/reviews/2026-09-17-verification-completion/shots/devices_settings.png): Device settings dialog modal.
14. [home_link_degraded.png](file:///c:/Users/vanth/Documents/MyDocument/Embedded/LVGL_UI/docs/reviews/2026-09-17-verification-completion/shots/home_link_degraded.png): Home view with degraded RF link (1 bar minimum floor).
15. [home_auto_paused.png](file:///c:/Users/vanth/Documents/MyDocument/Embedded/LVGL_UI/docs/reviews/2026-09-17-verification-completion/shots/home_auto_paused.png): Home view showing Auto paused due to timeout.
16. [devices_auto_thresholds.png](file:///c:/Users/vanth/Documents/MyDocument/Embedded/LVGL_UI/docs/reviews/2026-09-17-verification-completion/shots/devices_auto_thresholds.png): Device threshold editor dialog (Ventilation Fan).
17. [devices_settings_manual_only.png](file:///c:/Users/vanth/Documents/MyDocument/Embedded/LVGL_UI/docs/reviews/2026-09-17-verification-completion/shots/devices_settings_manual_only.png): Device settings for manual-only appliance.
18. [devices_auto_purifier.png](file:///c:/Users/vanth/Documents/MyDocument/Embedded/LVGL_UI/docs/reviews/2026-09-17-verification-completion/shots/devices_auto_purifier.png): Device threshold editor (Air Purifier / VOC).
19. [devices_auto_humidifier.png](file:///c:/Users/vanth/Documents/MyDocument/Embedded/LVGL_UI/docs/reviews/2026-09-17-verification-completion/shots/devices_auto_humidifier.png): Device threshold editor (Humidifier / RH).
20. [devices_auto_max_10000.png](file:///c:/Users/vanth/Documents/MyDocument/Embedded/LVGL_UI/docs/reviews/2026-09-17-verification-completion/shots/devices_auto_max_10000.png): Device threshold editor displaying upper limit (10,000 ppm).
21. [devices_auto_invalid_draft.png](file:///c:/Users/vanth/Documents/MyDocument/Embedded/LVGL_UI/docs/reviews/2026-09-17-verification-completion/shots/devices_auto_invalid_draft.png): Device threshold editor showing validation error on invalid draft.
22. [home_temp_humid_invalid.png](file:///c:/Users/vanth/Documents/MyDocument/Embedded/LVGL_UI/docs/reviews/2026-09-17-verification-completion/shots/home_temp_humid_invalid.png): Home view with invalid temperature and humidity readouts.
23. [dropdown_open_top.png](file:///c:/Users/vanth/Documents/MyDocument/Embedded/LVGL_UI/docs/reviews/2026-09-17-verification-completion/shots/dropdown_open_top.png): Dropdown open upwards.
24. [dropdown_open_bottom.png](file:///c:/Users/vanth/Documents/MyDocument/Embedded/LVGL_UI/docs/reviews/2026-09-17-verification-completion/shots/dropdown_open_bottom.png): Dropdown open downwards.

---

## 6. Architectural Boundary: UI Verification vs. MCU Target Constraints

The UI test suite and verifier provide mathematically rigorous verification of the UI layer under host simulation. The following physical concerns are explicitly partitioned as target firmware responsibilities:

1. **Flash Wear-Leveling Endurance:**
   - *UI Guarantee:* Commits are strictly event-driven (only upon explicit Save button press or manual mode change) with CRC protection and schema migration. No per-tick writes occur.
   - *Target Concern:* The physical Flash driver on the STM32/MCU must implement wear leveling, sector cycling, and brown-out power loss tolerance.
2. **True Random Jitter:**
   - *UI Guarantee:* 1000 ms periodic retransmissions and 3000 ms command timeouts are bounded by tick comparisons.
   - *Target Concern:* RF backoff and CSMA/CA MAC jitter must be applied in the SX1262 LoRa physical layer driver.
3. **RF Packet Loss Distribution:**
   - *UI Guarantee:* Link staleness masks data at 5000 ms, error phases latch at 3000 ms, and readbacks are validated with sequence numbers.
   - *Target Concern:* Multipath fading, burst error recovery, and RSSI/SNR packet header extraction are handled by the LoRa transceiver firmware.
