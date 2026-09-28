# Trends Autonomous Session History & Auto Live Path Delivery Report

Date: 2026-09-23  
Disposition: **IMPLEMENTED & VERIFIED**, ready for owner review.  
Scope: Consolidated delivery addressing all findings from [2026-09-23 investigation](../2026-09-23-trends-auto-investigation/REVIEW.md) and [CURRENT.md](../../prompts/CURRENT.md).

---

## 1. Executive Summary

This single consolidated delivery resolves both primary live path issues reported by the owner, with zero git commits, zero git pushes, and zero regressions across existing requirements:

1. **P1 — Live Trends Autonomous Session History:**
   - **Root Cause:** In the previous implementation, `update_trends_page_widgets()` directly read the "Current" live metric from `lora_node_status`, but Min/Max, plot series, timespan, and empty-state logic derived exclusively from `s_history`. In the codebase, `s_history` was only fed by `ui_feed_history()`, which was only called by simulator mocks and left as an integration checklist item for firmware.
   - **Resolution:** Implemented autonomous bounded session sampling in `ui.c`. On the very first accepted valid report after boot, the UI captures an initial singleton sample (`count = 1`), with Min/Max displaying that value and timespan displaying `"Single reading"`. Each elapsed session minute advances by 1 slot up to 16 points. Multi-minute gaps or RF drops are marked with `valid = false` (rendering clean chart gaps without false interpolation). Time labels format truthfully as elapsed session time (`+0m`, `+15m`, `+1h05m`) using `ui_font_18` ($\ge 18\text{px}$), avoiding unverified RTC wall-clock claims. When application firmware does call `ui_feed_history()`, it takes exclusive precedence and preserves wall-clock formatting (`%02u:%02u`). Zero heap allocation on the hot path (`ui_tick()`).

2. **P1 — Auto Command Triggering & Transition Hold Separation:**
   - **Root Cause:** In `ui_auto.c`, `ui_auto_rearm()` erroneously set `state->last_transition_ms = now;`. In `ui.c`, `commit_device_config_internal()` was calling both `ui_auto_rearm()` and `ui_auto_restart_hold()` upon selecting Manual $\rightarrow$ Auto. This restarted a 10-second hold even when no physical relay had switched. Additionally, 3 consecutive qualifying frames are required before triggering a state change.
   - **Resolution:** Removed the premature 10s hold from Manual $\rightarrow$ Auto mode switching and threshold limits changes in `commit_device_config_internal()`, and removed `last_transition_ms = now;` from `ui_auto_rearm()`. The 10-second anti-hunting hold now applies strictly to: (1) real confirmed physical relay transitions (echo sequence and reported GPIO match), (2) link reconnection, and (3) sensor recovery from faults. Switching to Auto arms immediately and evaluates on the next 3 qualifying frames without an artificial 10s delay. Preserved `ui_auto_restart_hold()` on explicit Retry button click to protect against rapid retry spamming.

3. **Humidifier Polarity & Rules Baseline:**
   - Confirmed Humidifier rule is intentionally inverted: turns ON when dry (RH $\le$ ON threshold, default 40%), turns OFF when moist (RH $\ge$ OFF threshold, default 50%). If ambient humidity is high (e.g. 55%), remaining OFF is the correct and intended behavior. Deadband (e.g. 45%) holds current state. Boundaries ($\text{RH} = 40\%$ and $\text{RH} = 50\%$) verified inclusive.

4. **Integration Flow & Serialized Contract:**
   - Validated the exact serialized contract for firmware: publish `lora_node_status`, increment `lora_rx_revision`, update `lora_last_rx_tick_ms`, call `ui_tick()`, and transmit `lora_hub_cmd` if `ui_is_tx_ready()` is true and `seq` changed or periodic 1000ms heartbeat elapsed.

---

## 2. Verification Gate Results

All 5 standard verification gates and regression checks were executed sequentially and passed 100%:

| Gate | Command | Result | Notes |
|---|---|---|---|
| **Simulator Build** | `tools/build_sim.bat` | **PASS** | Clean build with GCC (`-Wall -Wextra -Werror`). |
| **Regression Suite** | `tools/run_regression.bat -SkipBuild` | **PASS** | **35/35 checks passed (100%)**. 0 memory leaks across 50 cycles, 0 flushes on idle/identical frames, 27 scenario shots generated. |
| **Asset Check** | `python tools/generate_assets.py --check` | **PASS** | 25 canonical masters, 68 runtime icons verified. |
| **MCU Export** | `tools/export_mcu.bat --verify` | **PASS** | Export manifest verified. |
| **Production Verification** | `tools/verify_production.bat` | **PASS** | All 4 phases passed: collision safety, isolated Release build with `-Werror`, smoke test (exit 0), negative control (exit 1). |

---

## 3. CHECK 35 Test Details (`sim_pc/main.c`)

A comprehensive new verification check (`[CHECK 35] Autonomous Trends Session History & Live Auto Triggering`) was added to `sim_pc/main.c`:

1. **Part A — Autonomous Trends Session History:**
   - Verifies 0 count and empty box placeholder immediately on boot before any report arrives.
   - Verifies initial singleton point captured on first accepted report (`count = 1`, Min = Max = 450 ppm, span = `"Single reading"`).
   - Verifies 1-minute elapsed slot progression (`count = 2` after 60s leap and second report).
   - Verifies external feed override: calling `ui_feed_history()` on CO₂ sets CO₂ count to 3 and marks it as externally fed, while VOC remains autonomous with count 2.
   - Verifies gap handling: advancing 3 minutes with no frames leaves minutes 1 and 2 as gaps (`valid = false`) and captures minute 3 on packet resumption.
   - Verifies VOC = 0 baseline recognized as valid singleton.
   - Verifies time label formatting: autonomous displays `+0m` / `+%uh%02um` using `ui_font_18`.

2. **Part B — Auto Command Triggering without 10s Delay:**
   - Configures Fan (Device 1) from Manual $\rightarrow$ Auto.
   - Feeds Frame 1 (CO₂ = 1200): qualify count = 1, command phase remains IDLE.
   - Feeds Frame 2 (CO₂ = 1205): qualify count = 2, command phase remains IDLE.
   - Feeds Frame 3 (CO₂ = 1210): command triggers immediately (`phase == PENDING`, `desired_gpio == 1`), proving elimination of the 10s hold bug.
   - Confirms transition via Node readback echo (`seq_echo` matched, relay reported 1). Phase clears to IDLE.
   - Verifies confirmed transition hold: opposite threshold (CO₂ = 600 $\le$ 800) is suppressed during the 10s post-switch hold.
   - Verifies expiration of hold: after 10s elapses, 3 fresh frames at CO₂ = 600 command OFF (`desired_gpio == 0`).

3. **Part B2 — Active Low Polarity:**
   - Configures Purifier (Device 0) with `active_low = 1` and switches Manual $\rightarrow$ Auto.
   - Feeds 3 frames at VOC = 200 ($\ge 150$).
   - Verifies command triggers on frame 3 with wire GPIO level 0 (logical ON for active low).

4. **Part C — Humidifier Thresholds & Boundaries:**
   - Humidifier configured in Auto (`on_thresh = 40%`, `off_thresh = 50%`).
   - High RH = 55% while OFF: correctly remains OFF.
   - Deadband RH = 45%: correctly remains OFF.
   - Dry RH = 35% ($\le 40\%$): 3 frames trigger ON command (`desired_gpio == 1`).
   - Boundary tests: RH = 40% triggers ON; RH = 50% triggers OFF.

5. **Part E — Serialized Live-App Integration Sequence:**
   - Simulates firmware event loop: `lora_node_status` published $\rightarrow$ `lora_rx_revision++` $\rightarrow$ `lora_last_rx_tick_ms` updated $\rightarrow$ `ui_tick()` called in same serialized context $\rightarrow$ `lora_hub_cmd` observed and verified $\rightarrow$ Node status echo received and reconciled.

---

## 4. Performance & Memory Impact

- **Heap Allocation on Hot Paths:** Strictly **0 bytes**. Sampling occurs in statically declared buffers in `ui.c`.
- **Idle Render Rate:** Strictly **0 flushes/sec, 0 pixels pushed** on identical frames and sub-second idle.
- **Stable 50-Cycle Heap Stress:**
  - Peak Used: 81%
  - Min Free Heap: 8,296 bytes (comfortably above the 4,096 B safety margin)
  - Heap Drift across cycles 10 to 50: **+0 bytes** (zero leak).
- **Typography:** All rendered copy strictly uses fonts $\ge 18\text{px}$ (`ui_font_18`, `ui_font_24`, `ui_font_34`).
- **Wire Contract:** `lora_hub_cmd_t` (8B) and `lora_node_status_t` (20B) unchanged.
