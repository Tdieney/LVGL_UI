# LVGL UI Implementation Plan

## Phase 1: Architecture Setup
- [x] Create `ui.h` and `ui.c` (Main coordinator)
- [x] Create `screens.h` and `screens.c` (Screen layouts and widgets)
- [x] Create `actions.h` and `actions.c` (Event handlers & lazy loading logic)
- [x] Create `images.h` + `ui_image_dial.c` (Assets generated via `tools/build_assets.bat`)

## Phase 2: Common Components & Layout
- [x] Implement Top Bar (Status, Title, Link Status)
- [x] Implement Tab Bar (Dashboard, Monitor, Control, Graphs, Diagnostics, Settings)
- [x] Implement Page Container structure
- [x] Fix container double-padding offsets
- [x] Fix Tab Bar overflow via `flex-grow`
- [x] Sync exact color hierarchy with reference design

## Phase 3: Screen Implementations
- [x] Implement Dashboard Screen
- [x] Implement Monitor Screen
- [x] Implement Control Screen
- [x] Implement Graphs Screen
- [x] Implement Diagnostics Screen
- [x] Implement Settings Screen
- [x] Fix widget clipping (sliders) via `OVERFLOW_VISIBLE`
- [x] Fix Monitor screen flex gaps (`pad_row`) to prevent clipping

## Phase 4: Memory & Tick Architecture
- [x] Implement Single Screen SPA Architecture (`ui_MainScreen`)
- [x] Instant Memory Cleanup via `lv_obj_clean` (Eliminates OOM spikes)
- [x] Add active-screen guards (`ui_current_tab`) to `tick_screen_XXX` to prevent pointer crashes
- [x] Add `ui_tick()` logic for updating dynamic widgets
- [x] Wire up state variables

## Phase 5: Performance & Demo Upgrade (Detailed in PERF.md)
- [x] PC simulator (`sim_pc/`, GDI backend + headless screenshot `--shot` mode)
- [x] Two-lane tick scheduler: SLOW 5 Hz (text) / CHART 8 Hz round-robin
- [x] Value-change guards (`label_set_if_changed`) + `fmt_scaled` (zero float printf)
- [x] Pipeline assets: `tools/gen_icons.py` + `tools/png2lvgl.py` + `lv_font_conv`
- [x] Custom fonts: JetBrains Mono (66/44/30/22/20px)
- [x] Wire all actions directly to global wire structs (`motorCmd`)
- [x] `demo_sim.c` — Standalone telemetry simulation for Demo mode (`UI_DEMO_SIM=1`)
- [x] Upgrade all 6 screens per spec + tab active highlight + boot splash
- [x] Diagnostics: fault register bitfields (`motorStatusSlow.bits.faults`)
- [x] Measure per-screen heap footprint on simulator (`docs/MCU_MEMORY_PROFILE.md`)

## Phase 6: Operator UX Pass
- [x] Update color palette to light mode & high-contrast tokens
- [x] Rename HMI title to "MOTOR CONTROL HMI"
- [x] Monitor redesign: Streamline metrics into essential rows with 20px+ font floor
- [x] Control: Speed setpoint slider + Position ring knob
- [x] Dashboard: Power and motor temp cards + motor watermark

## Phase 7: Semantic Color Palette & Accessibility
- [x] High-contrast state colors: Green (`COLOR_OK`), Red (`COLOR_DANGER`), Orange (`COLOR_WARN`)
- [x] Industrial button conventions: START (Green), STOP (Red)
- [x] Reserve Azure accent for hero elements (Gauge, active tab underline, voltage chart)
- [x] Full contrast pass for low-cost IPS/TFT displays (high-contrast text ramps & crisp card borders)

## Phase 8: Control Screen Refinement
- [x] Segmented mode selection & dropdown layout
- [x] Position control: Hand-rolled gapless 360° ring knob with integer arc math (`ctrl_pos_math.h`)
- [x] Torque control directly in mN·m (0..11000 mN·m range)
- [x] Mode-specific limit inputs (`limitRaw`) for velocity and current limits

## Phase 9: Single-Screen SPA Hardening & Memory Footprint
- [x] Single resident content screen architecture (`lv_obj_clean` before rebuild)
- [x] Coalesced and rate-limited screen tab transitions from `ui_tick()`
- [x] Zero dynamic string allocation during refresh loops (`label_bind_buffer()`)
- [x] Static captions trỏ Flash memory (`lv_label_set_text_static()`)
- [x] Validated MCU memory profile: **52 KB LVGL Heap** + **16 KB Partial Draw Buffer** (`ui_mcu_profile.h`)

## Phase 10: Telemetry & Demo Integration
- [x] Strict separation between UART wire structs (`MotorCmd_t`, `MotorStatusFast_t`, `MotorStatusSlow_t`) and Demo telemetry
- [x] Long-press demo toggle (~800ms) with STOP-state interlock
- [x] Standalone integer-only math for demo telemetry (`sin_lut`, zero soft-float calls)
- [x] 120-second automated regression test suite (`tools/run_regression.bat`)

## Backlog / Future Tasks
- [ ] Shared `lv_style_t` objects for cards and pills to further save 10–15 KB heap if required
- [ ] Target-side UART driver integration (external to UI repo — refer to `PERF.md`)
- [ ] On-device parameter editing in Settings (spinbox/keypad integration if requested)
