# CLAUDE.md

> Historical Claude working context. For current implementation invariants,
> the validated 52KB/800x10 memory profile, and Demo telemetry isolation, read
> `AGENTS.md` and `docs/MCU_MEMORY_PROFILE.md` first.

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project overview

LVGL v8.4.0 C source for an 800x480 RGB565 industrial HMI (7" touchscreen) controlling a BLDC motor via
FOC. Target hardware: **80MHz MCU, 1MB flash, 128KB RAM**, display over **xSPI (TR230S module with its own
GRAM)**, GT911 I2C touch. The xSPI bus is the bottleneck — every design rule here exists to minimize dirty
regions. Read `PERF.md` before adding widgets: it documents the measured heap budget (worst screen is at
64% of the 64KB LVGL heap) and the techniques that must be preserved.

Two secondary parts:

1. **`Industrial HMI for BLDC Motor/`** — a Figma-exported React + Vite prototype that is the **visual
   spec** the LVGL code was ported from (colors, layout, behavior). Throwaway mockup; don't extend it.
   Cross-check pixel values against `src/app/App.tsx` when styling LVGL widgets.
2. **`sim_pc/`** — Windows PC simulator (own GDI backend, no SDL). This is how changes are verified: build,
   screenshot headlessly, look at the PNG. The sim's `lv_conf.h` mirrors the MCU heap (64KB) on purpose so
   out-of-memory shows up on PC first — a heap overflow here crashes in `lv_obj_class_create_obj`.

## Commands

- **Build simulator** (needs `C:\Toolchains\w64devkit` gcc + CMake + Ninja, all installed):
  ```
  export PATH="/c/Toolchains/w64devkit/bin:$PATH"
  cmake -S sim_pc -B sim_pc/build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=C:/Toolchains/w64devkit/bin/gcc.exe
  cmake --build sim_pc/build
  ```
- **Run interactive demo**: `sim_pc/build/sim_pc.exe` — the demo simulator now boots **OFF** (idle /
  disconnected); **HOLD "MOTOR CONTROL HMI"** (top-left) ~800ms to toggle it on / off (a deliberate
  long-press, NOT a tap — see the demo-toggle bullet below for why). Or launch with **`--demo`** to start
  it on (a direct `demo_sim_toggle()` call, bypasses the gesture).
- **Headless screenshot** (primary verification loop):
  ```
  sim_pc/build/sim_pc.exe --shot <dashboard|monitor|control|graphs|diagnostics|settings> out.raw --ms 15000 --demo
  python tools/raw2png.py out.raw out.png
  ```
  `--ms` is *virtual* time (runs instantly). It also prints LVGL heap usage — check it after adding widgets.
  **Add `--demo`** or the shot shows the idle/disconnected boot state (demo defaults off). Use `--ms 600` to
  capture the boot splash. Reference set lives in `docs/screenshots/` (captured with `--demo`).
- **Regenerate assets** (splash logo + fonts — the gauge is live `lv_arc` now, no dial image): `tools\build_assets.bat` (needs Python+Pillow and Node/npx).
- **Export for the MCU project**: `tools\export_mcu.bat [D:\path\to\mcu_project]` — zips the firmware-relevant
  files (root UI sources + `fonts/` + `PERF.md`) into `hmi_ui_<timestamp>.zip`; with a destination argument it
  also copies the zip there and extracts into `<dest>\hmi_ui\`.
- React prototype: `cd "Industrial HMI for BLDC Motor" && npm i && npm run dev`. No tests/linter anywhere.

## Architecture

- **Single-screen SPA**: one `ui_MainScreen`; tab switch = `lv_obj_clean()` + `create_screen_XXX()` +
  `ui_force_refresh()` + `ui_tabbar_set_active()`, all centralized in **`ui_build_tab()` (`ui.c`)**. Top bar +
  tab bar live once on `lv_layer_top()` and survive cleans. `ui_current_tab` is the source of truth; every
  `tick_screen_XXX()` returns early unless its tab is active (stale widget pointers after clean would crash
  otherwise). Keep this guard.
- **Tab switches are COALESCED + THROTTLED (`ui.c`, do not undo)**: the tab buttons' `action_tab_XXX()` only
  call `ui_request_tab(tab)` — a cheap flag set. The actual `lv_obj_clean`+rebuild happens in
  `ui_service_pending()` at the top of `ui_tick()`, at most once per tick and no more often than
  `UI_TAB_MIN_MS` (90ms). WHY: the old path rebuilt the whole screen *inside* the click callback, so fast
  repeated tapping (found by the user stress-testing + demo mode) piled up ~30-60KB alloc/free cycles per
  tap and fragmented the 64KB LVGL heap until a `create` failed and the UI hung — recoverable only by
  resetting the MCU. Coalescing bounds rebuilds to one-per-tick of the *final* tab. Headless `--shot` builds
  the requested tab lazily on the first `ui_tick`, so `main.c` samples the heap AFTER the virtual-time loop.
  Two more hardening layers guard the same demo-lockup risk (keep them): **(a) shared `lv_style_t`**
  (`styles_ensure()`/`style_slider()` in `screens.c`) — sliders, action/dir buttons, and Monitor/Settings
  list-row dividers use `lv_obj_add_style` with process-lifetime static styles instead of per-object local
  style properties, cutting per-rebuild allocation + fragmentation (measured Control frag 10%→4%); per-state
  colors set by ticks stay local (local > added style). **(b) circuit breaker** in `ui_build_tab`: after the
  clean, if `lv_mem_monitor` reports `free_size < UI_HEAP_FLOOR` (20KB) it builds a NULL-guarded "LOW MEMORY /
  reset" fallback instead of a real screen whose mid-build alloc would return NULL and be deref'd. **Full
  widget-reuse (all 6 screens resident, hide/show instead of clean+rebuild) is NOT feasible** — 6 resident
  screens far exceed the 64KB heap; the single-screen clean+rebuild is deliberate, coalescing + (a) are the
  realistic substitute.
- **`ui_force_refresh()`** (`ui.c`/`ui.h`): `create_screen_XXX()` builds widgets in a default look (e.g. a
  mode button unselected) and normally waits for the next scheduled `tick_screen_XXX()` call to style them
  to match `ms` — but the SLOW lane only runs every 200ms, so without this call a tab switch briefly renders
  the wrong state (found via user report, not visible in the PC sim's headless `--shot` mode since the boot
  splash overlays the first ~2.5s regardless of tab). `ui_build_tab()` calls
  `create_screen_XXX()` then `ui_force_refresh()` before `ui_tabbar_set_active()` — keep this order when
  adding a new tab (add the new `case` to `ui_build_tab()`'s switch, not a bespoke `action_tab_XXX`).
- **Top bar (60px) + tab bar (56px)**, both bumped from 44/36px for the 20px+ type floor. Top bar left
  side: brand mark (2 stacked bars, ink+`COLOR_BRAND_RED`) + "MOTOR CONTROL HMI" title — the page-name +
  divider that used to sit next to the title were **removed** (no room left at this type scale next to a
  readable MOTOR state + link pill on the right, and the active tab is already highlighted one bar down).
  Tab bar buttons carry an icon + a short label (DASH/MON/CTRL/GRAPH/DIAG/SET) — full words didn't fit
  6-across at 20px. The tab icons are **custom `ALPHA_4BIT` images** (`ui_icon_dash`/`_mon`/`_ctrl`/`_graph`/
  `_diag`/`_set`, drawn by `tools/gen_icons.py` to match the finalized mockup's hand-drawn line-icon SVGs),
  NOT FontAwesome glyphs — a user review found the FA glyphs didn't match the mockup shapes. They're
  alpha-only so `create_tab_btn()` recolors one asset per state via `img_recolor` (idle `COLOR_TEXT_L` /
  active `COLOR_ACCENT`), exactly like the old font glyph. `create_tab_btn()` takes `const lv_img_dsc_t *`
  for the icon + the label text. Link pill text is `"RS-485 | <baud>"` (was `"UART | 115200"`).
- **ALL icons in the app are custom `ALPHA_4BIT` images now — the FontAwesome font `ui_font_icons20` was
  removed** (a later review extended the tab-icon treatment to the Monitor rows + Diagnostics rows/marks,
  after which nothing referenced the FA font). `tools/gen_icons.py` draws the 14 currently used icons
  (6 tabs + bolt/thermo/link + check/xmark + chevron + cw/ccw) as alpha masks matching the
  mockup SVGs — 24×24 by default, but a per-icon `SIZES` override renders **cw/ccw at 34×34** (the direction
  rotation arrows sit centered on a 52px button, and alpha images can't be zoomed at runtime, so a bigger
  button icon must be drawn bigger natively). `cw`/`ccw` are chunky open "redo/undo" rings with a SOLID
  triangular arrowhead; CCW is the exact horizontal mirror of CW.
  `images.h` declares them as `ui_icon_*`. Recolor via `img_recolor`+`img_recolor_opa=COVER` (Monitor/Diag
  type icons → `COLOR_TEXT_VL`; Diag status mark → `ui_icon_check` green / `ui_icon_xmark` red, swapped with
  `lv_img_set_src` in the tick). Need a new icon? add a draw fn in `gen_icons.py`, a png2lvgl line in
  `build_assets.bat`, an `LV_IMG_DECLARE` in `images.h`, and the file to `export_mcu.bat`. The `UI_SYMBOL_*`
  string macros are gone; `LV_SYMBOL_*` (built-in Montserrat, on the Settings SAVE/DEFAULTS buttons) remain.
- **Control screen shows one setpoint control at a time, matching the confirmed `opMode`** — vertical zones
  (the old 240px "OPERATING MODE" column is GONE): **(1)** the mode selector is a compact **`lv_dropdown`**
  (`ctrl_mode_dd`, "MODE" + SPEED/TORQUE/OPEN LOOP/POSITION) pinned to the card's **top-right corner** (freeing
  the whole central space); **(2)** a white card holding **two overlapping full-size layouts, only one un-HIDDEN
  by the tick**; **(3)** an equal-width `START`/`STOP`/`CALIBRATE` bar (all three `create_action_btn`,
  `flex_grow 1`). The dropdown's visual order ≠ `OpMode_e` order → map through `ctrl_mode_order[]`; `ctrl_mode_dd_cb`
  sets opMode+zeros **both** ctrlValRaw and limitRaw, and the tick syncs the dropdown's selection to the confirmed opMode. It uses the
  `ui_icon_chevron` image as its symbol (the built-in `LV_SYMBOL_DOWN` only exists in Montserrat — see the
  dropdown gotcha).
  - **LEVEL layout** (`ctrl_level_layout`, for SPEED/TORQUE/OPEN LOOP): the LEFT is a **two-column SPEED | TORQUE
    readout** (user: no motor status, no watermark). `garea` is a flex ROW (hairline right border); two
    **equal `flex_grow` cells** (`scol` | `tcol`, hairline divider between), each a flex COLUMN aligned
    **top + left**, `pad_all 22`, `pad_row 10` (generous spacing): a `mono20` label pinned to the TOP, big
    `mono66` number (`ctrl_spd_num` / `ctrl_tq_num`, ~50% bigger hero — TORQUE reads the real `actualTorque` via
    `motor_torque_nm()`), `mono20` unit, a flex-grow spacer, then a **level bar** at the bottom
    (`ctrl_spd_bar` / `ctrl_tq_bar`, `lv_bar` 0..100; fill recolored by band green/amber/red on a transition —
    speed vs UI_MAX_RPM, torque vs peak 11 N.m). All text LEFT-aligned. SLOW lane only (FAST is a no-op).
    Run-state is on START/STOP, direction on the CW/CCW rotate-icon buttons — NOT duplicated here. English UI text. (Earlier tries —
    E2 card w/ watermark+chip, and a centered two-number layout — were rejected.)
    RIGHT = a compact column (`rcol`, width `CTRL_RCOL_W` 280, `pad_top`
    clears the floating dropdown): a row with a **generic "TARGET" title left** (`ctrl_lvl_title`) + **value+unit
    right** (`ctrl_lvl_num`+`ctrl_lvl_unit`) — the title is deliberately generic (the dropdown names the mode,
    the unit names the quantity); a **plain slider** (`ctrl_lvl_slider`, NO steppers); then a **LIMIT row**
    (`ctrl_lim_title`/`ctrl_lim_num`/`ctrl_lim_unit` + `ctrl_lim_slider`, same structure as TARGET) — the
    secondary cascade limit (`limitRaw`); then the **direction buttons** (`create_dir_icon_btn` — a CW / CCW
    rotate icon each, no text; evolution was FWD/REV → FORWARD/BACKWARD → rotate icons, because FWD/REV read
    oddly for a spinning motor, the full words were too long, and CW/CCW-as-text too cryptic for non-engineers
    — a rotate arrow is compact and universally legible; `create_dir_icon_btn` centers a recolorable alpha icon,
    tick swaps `img_recolor` white-on-accent / ink-on-white per state). **Layout: `rcol` is `SPACE_BETWEEN`
    with TARGET (title+slider) and LIMIT (title+slider) each wrapped in their own flex group** (`pad_row 14`
    keeps a title just above its slider) **and the direction row as the third group** — so the three groups
    spread evenly down the column with no dead gap, and each title stays attached to its slider (replaced an
    earlier two-grow-spacer approach whose gaps read unevenly). **MODE dropdown, TARGET group, LIMIT group,
    and the direction buttons all share `CTRL_RCOL_W` with aligned left edges** (the `dd_wrap` is that
    width, right-aligned in the card, so it lines up with `rcol` below — user request). ONE reused primary +
    ONE reused limit slider/unit — `tick_screen_control` reconfigures per `ctrl_lvl_mode`: primary
    SPEED→`RPM`/0..`UI_MAX_RPM`, **TORQUE→`N.m` (float, `raw/1000`; slider still runs 0..`UI_MAX_TORQUE_MNM`
    in mN·m)**, OPEN LOOP→`%`/0..100; limit SPEED/OPEN LOOP→`CURRENT LIM`/`A`/0..`UI_MAX_CURRENT_MA` (mA→A
    float), TORQUE→`SPEED LIM`/`RPM`/0..`UI_MAX_RPM`. `ctrl_setpoint_cb` dispatches to the per-mode
    `action_*_change`; `ctrl_limit_cb` writes `limitRaw` for the level modes (`action_motor_speed_change` is
    also the Dashboard slider's handler).
  - **POSITION layout** (`ctrl_pos_layout`): a flex ROW mirroring LEVEL — **gray ring + dot on the LEFT
    (`parea`, flex_grow 1, shifted left of centre), a `SPEED LIM` column on the RIGHT** (`pcol`, `CTRL_RCOL_W`,
    same pad_top as `rcol`: `ctrl_pos_lim_num` + `ctrl_pos_lim_slider`, `RPM`/0..`UI_MAX_RPM`, `ctrl_pos_limit_cb`
    → `limitRaw`). The ring is `ctrl_pos_ring` — an `lv_arc` `bg_angles(0,360)` (full ring), `rotation 270`
    (value 0 at top), range `0..UI_POSITION_MAX_RAW` (3600) so a full turn = a target **angle 0..360.0° at
    0.1°/LSB**, mapped 1:1 onto `ctrlValRaw`; INDICATOR `opa` 0 (no value fill) AND the on-stroke knob removed.
    The dot is a **separate** `lv_obj` (`ctrl_pos_dot`, **charcoal `COLOR_ACCENT`** — an interactive handle, not
    azure data; user request) that `ctrl_pos_update_dot()` places at radius `CTRL_POS_DOT_R` inside the rim.
    **A target-angle NUMBER now sits in the ring centre** (`ctrl_pos_num` `mono44` "180.0" + a `mono20` `°` —
    mono44 has no `°` so the degree glyph is a separate label; `ctrl_pos_update_readout()` formats `raw/10`);
    the old "no text" spec was dropped (user request: operator must see the exact target angle).
    **Touch drag is HAND-ROLLED, not lv_arc's built-in** (this was the erratic jump bug fix): the ring is
    **display-only** (`LV_OBJ_FLAG_CLICKABLE` CLEARED) and **`parea`** is the touch target (`ctrl_pos_drag_cb`
    on PRESSED+PRESSING — bound to `parea` not the whole layout so the right-column limit slider isn't treated
    as a ring drag; `ctrl_pos_area` static holds it for the tick's drag guard). It reads the pointer, and
    `ctrl_pos_angle_to_raw()` in the LVGL-free header **`ctrl_pos_math.h`** maps the angle continuously to
    0..3600 (top=0, clockwise). WHY: lv_arc's own drag has an anti-wrap heuristic (`lv_arc.c`: `if
    (LV_ABS(delta_angle) > 280) angle = 0 or deg_range`) that SNAPS the value to min/max when the finger
    crosses the top seam of a gapless 360° arc — that was the jump. The hand-rolled mapping is continuous.
    Unit-tested in **`tests/test_pos_angle.c`** (host gcc, no LVGL): asserts cardinals + a full 0..359° sweep
    with max interior step ~1 quantum and no seam snap. No direction buttons.
  - **`ctrlValRaw`/`limitRaw` display guard is preserved**: the reused level panel shows `ctrl_lvl_mode`, so it
    trusts `motorCmd.ctrlValRaw` **and `motorCmd.limitRaw`** only when `motorCmd.opMode == ctrl_lvl_mode` (else
    shows 0) — the shared-field hazard (UART_PROTOCOL.md sec 2.1/2.4) applies to BOTH fields, keep it. `UI_MAX_TORQUE_MNM` is now the **real** 11000 (11 N·m,
    user-confirmed), and the torque slider runs **directly in mN·m** (`action_torque_change` writes it straight
    to `ctrlValRaw`, no %→mN·m conversion).
  - **`START`/`STOP` emphasis flips with the confirmed run-state** (`ctrl_last_running`): stopped → START solid
    green + STOP red-outline; running → STOP solid red + START green-outline + **`CALIBRATE` disabled**
    (`LV_STATE_DISABLED` + dimmed). All three are equal width (user request — CALIBRATE is no longer narrowed).
  - **Colors**: the actualRpm gauge (value arc + hub) uses the **green speed band** (`band_color()`), same as
    Dashboard; the **POSITION dot is now charcoal `COLOR_ACCENT`** (was azure — user request), so the Graphs
    VOLTAGE series is the only azure (`COLOR_GAUGE`) left in the app. See the color rule below.
  - **Heap ~62%** (~25KB free) — up from ~54% after the per-mode LIMIT row/slider + POSITION degree readout +
    speed-limit column were added; still in line with the other screens. The earlier PID
    column, FOC-info card, PWM/FOC pill, the −/+ steppers / FINE ADJUST / GO TO 0, and the top segmented mode
    row were all dropped by progressive user requests; PWM/FOC state lives only on Monitor.
- **Diagnostics = summary banner + a single 3-column FAULT REGISTER grid** (`LV_LAYOUT_GRID`, 3 `FR(1)`
  columns × 3 `GRID_CONTENT` rows, row-major so the 7 fault bits read left-to-right/top-down; grid cells
  MUST set `lv_obj_set_height(cell, LV_SIZE_CONTENT)` or they keep `lv_obj`'s default height and the rows
  balloon — this bit once). The banner (`tick_screen_diagnostics`) is a health rollup: green
  "ALL SYSTEMS NORMAL / 7 / 7 OK" when clean, red "FAULT DETECTED / (7-N) / 7 OK" when any bit is set. Each
  fault cell is `type-icon + name + status-mark` (check→green / x→red + blink). Two earlier layouts were
  dropped to reach this: "Sensor Status" (6 rows — 3 duplicated fault bits, 3 never wired to anything) and
  the "Warning Conditions" panel (high-temp/low-bus/comm-timeout). The finalized mockup has neither, so
  neither is in the UI — the warning *thresholds* no longer surface anywhere; if you want them back, fold
  them into the banner rather than re-adding a separate panel (see PLAN.md Phase 22 backlog).
- **Settings' UART section is now "RS-485 CONFIGURATION"** with a plain-text `INTERFACE: RS-485` row (not a
  dropdown — it's fixed hardware) and baud options extended to `921600` (was capped at `460800`). The old
  "MODBUS / PROTOCOL" card (Protocol/Frame/Timeout/Retries) is gone — entirely made-up display text; this
  repo's real framing is `motor_comm.h`'s SOF/ID/LEN/PAYLOAD/EOF, not Modbus RTU, and Timeout/
  Retries were never wired to anything. Don't recreate a fake-data card like it. **The BAUD/PARITY/STOP-BITS
  dropdowns are now LIVE and exported** (`ui_rs485_baud`/`ui_rs485_parity`/`ui_rs485_stopbits`, `extern` in
  `screens.h`, defined in `screens.c`): each dropdown inits FROM the global and writes back on change, so
  `main.c` reads them to configure the real UART (and may seed them before `ui_init()` for a non-921600
  default). `ui_rs485_baud` holds the ACTUAL baud (not the index); the top-bar "RS-485 | <baud>" pill and
  `tick_common_ui` track it. See UART_PROTOCOL.md sec 2.5.
- **Gauge easing**: the FAST lane doesn't snap the gauge to `actualRpm` — it eases a float (`dash_gauge_ease`
  / `ctrl_gauge_ease`, factor `GAUGE_EASE 0.35`) toward it each tick, so the arc/needle/hub glide between
  telemetry samples (reset to the current value on screen create, no entry sweep). The RPM *number* still
  shows the exact reading. Pairs with `demo_sim` running at **100ms** (was 200) for finer motion.
- **Two-lane tick scheduler** (`ui_tick()` in `ui.c`): SLOW 200ms (all text labels + `tick_common_ui()` for
  the top bar) + CHART 125ms (graphs round-robin → each series 2Hz, staggered). **The old FAST 50ms lane was
  retired** — it only drove the Dashboard/Control gauge arc+needle easing, and those became plain-text tiles,
  so every `tick_screen_*()` now early-returns on `UI_LANE_FAST` and dispatching it was 20 no-op wakeups/sec.
  The `UI_LANE_FAST` enum + the early-return guards remain (harmless); if a future widget needs sub-200ms
  motion, re-add a lane in `ui_tick()` rather than speeding SLOW up — that would multiply the xSPI
  dirty-region load, which is the bottleneck. Telemetry itself arrives at 100ms (`motorStatusFast`) /
  1000ms (`motorStatusSlow`), so 200ms text is already inside the data rate.
  `dispatch_tick()` calls the ACTIVE screen's tick only — **except `tick_screen_graphs()`,
  which runs on EVERY tick regardless of tab** so it can keep 4 rolling history buffers (`g_hist`, size =
  chart point count) filled; on CHART lane it appends the round-robin series to its buffer always, and
  renders the chart only when Graphs is the active tab (its `chart_*` pointers are dangling otherwise —
  freed by `lv_obj_clean`). `create_screen_graphs()` pre-draws each chart from its buffer (`graph_prefill`)
  so Graphs is never empty on entry. New periodic UI work must join a lane, never run unthrottled.
- **Redraw guards** (`screens.c` helpers): `label_set_if_changed()` (skip identical text — no
  invalidation), `label_color_if_changed()`, per-widget band/state caches (only restyle on transitions),
  `fmt_fixed()` (fixed-point formatting, no float printf). Dynamic value labels get **fixed sizes**
  (`lv_obj_set_size` + `LV_LABEL_LONG_CLIP`) so text changes never trigger flex re-layout.
- **No bridge struct — the UI reads/writes the 3 wire structs from `motor_comm.h` directly**
  (user's explicit choice, replacing an earlier `ms_t` design): `motorCmd` (Tx, `MotorCmd_t` — `actions.c`
  writes `.bits.cmd/.dir/.opMode/.ctrlValRaw/.limitRaw` straight into it; **frame is now 6 bytes** —
  `MotorCmd_t` gained a 16-bit `limitRaw` secondary-limit field, container became `uint64_t`/`bytes[8]`, TX
  `bytes[0..5]`, `MOTOR_CMD_LEN=6`; see UART_PROTOCOL.md sec 2.4), `motorStatusFast` (Rx 100ms,
  `MotorStatusFast_t`), `motorStatusSlow` (Rx 1000ms, `MotorStatusSlow_t`), all declared in `screens.h`/
  defined in `screens.c`. Firmware's entire job re: motor comms is transmit whatever is currently in
  `motorCmd` on its own timer, and fill the two Rx structs from parsed UART bytes — no translation layer.
  Unit conversion (mA→A, mV→V, 0.1%→%, raw signed 0.1°C→°C via `MotorStatusSlow_GetMtTemp/GetInvTemp`)
  happens **inline at each display site** in `screens.c`, not in one central place — if you add a new
  reading of one of these fields, convert it there, don't reintroduce a cached/converted copy.
  `motor_is_running()` and `motor_power_w()` (`screens.h`) are the only two derived helpers — `power` isn't
  a wire field (voltage × current, computed on read) and there's no separate "running" bit (derived from
  `motorStatusFast.bits.motorState`). `motorConnected` is a 4th plain global, deliberately NOT part of any
  wire struct — set to 0 by whoever parses UART (or `demo_sim.c`) on a receive timeout.
- **`ctrlValRaw` (and now `limitRaw`) is one field shared by every `opMode`** (SPEED/TORQUE/OPEN_LOOP/POSITION
  all reuse both, meaning depends on which mode is active — see `motor_comm.h`; `limitRaw` =
  current-limit mA for SPEED/OPEN_LOOP, speed-limit RPM for TORQUE/POSITION). This bit a real bug once already:
  displaying it unconditionally on both the speed slider and the torque slider let one reinterpret the
  other's leftover value in the wrong unit (250 RPM read back as "125%" torque). Every site that reads
  `ctrlValRaw` for display **must** first check `motorCmd.bits.opMode` matches that control's own mode,
  showing 0 otherwise (see `tick_screen_dashboard`/`tick_screen_control` in `screens.c`) — don't remove that
  guard. `action_mode_select()` additionally zeros `ctrlValRaw` on every mode switch, but that's a
  *separate* safety net for what gets transmitted, not a substitute for the display-side check (the boot
  default sets `opMode`+`ctrlValRaw` directly, bypassing that reset entirely).
  **Safe power-on (`ui_init` in `ui.c`): `motorCmd` boots fully idle — `cmd=0` (STOP) + `ctrlValRaw=0` (zero
  setpoint)** so the motor can NEVER spin just because the UI powered up; the operator must set a value and
  press START. Don't seed a non-zero boot setpoint here (a plugged-in board would then jump to it on the
  first START). Telemetry seeds (voltage/temps/`motorConnected`) also boot at 0/disconnected until the first
  real frame — `demo_sim` (sim only) injects its own setpoint + fills telemetry at auto-start so the
  standalone sim still looks alive without the real boot state being unsafe.
  Per-mode `ctrlValRaw` units are all **real, confirmed specs** now (no placeholders): SPEED = RPM (1/LSB,
  0..`UI_MAX_RPM`), TORQUE = mN·m (1/LSB, 0..`UI_MAX_TORQUE_MNM` = 11000 = 11 N·m — the torque slider runs
  directly in mN·m, no %-conversion), OPEN_LOOP = PWM duty (0.1%/LSB, slider ×10), **POSITION = angle
  (0.1°/LSB, 0..`UI_POSITION_MAX_RAW` = 3600 = 360.0° — the full-circle knob maps 1:1 onto ctrlValRaw)**.
  `UI_MAX_RPM` lives in `screens.h` (not `screens.c`) specifically so `demo_sim.c` reads the same constant
  instead of maintaining its own copy — that duplication (`UI_MAX_RPM`/`SIM_MAX_RPM`) is exactly what let
  the two drift out of sync before. See `UART_PROTOCOL.md` for the full field-mapping table and open gaps
  (config read/write, and `CLEAR_FAULTS`/`RUN_CALIBRATION`/`SAVE_CONFIG`/`LOAD_DEFAULTS` — these 4 actions
  are TODO stubs in `actions.c` since `motor_comm.h` has no message for any of them yet).
- **`demo_sim.c`** (compiled only with `UI_DEMO_SIM=1`, which sim_pc defines): fabricates
  `motorStatusFast`/`motorStatusSlow` so the HMI looks alive with no hardware. **Runtime on/off via
  `demo_sim_toggle()`, default OFF at boot** — while OFF `sim_step` does nothing so the real UART parser owns
  the three wire structs; toggling OFF also resets to a safe idle (motor commanded STOP, telemetry cleared,
  link shown disconnected). **Demo boots IDLE — no auto-start** (user choice): enabling it commands STOP +
  zero setpoint and the operator presses START on the UI; voltage still sweeps so the Graphs stay lively even
  while stopped. The top bar wires the toggle to a **~800ms HOLD on "MOTOR CONTROL HMI"** — a deliberate
  long-press (`topbar_demo_toggle_cb` on PRESSED/RELEASED/PRESS_LOST: flips only if held ≥ `DEMO_HOLD_MS`,
  with a `DEMO_DEBOUNCE_MS` re-flip guard, PRESS_LOST cancel, and ONE `ui_force_refresh()` after the flip),
  all guarded by `#if UI_DEMO_SIM`. **WHY a hold, not a tap (demo-stability bug fix):** the toggle switches
  ownership of the 3 wire structs between `demo_sim` and the real UART Rx path (ISR-filled on hardware). A
  stray single tap on the brand — easy to hit while pointing at the screen during a partner demo — used to
  flip modes instantly; the mid-demo handoff left the panel looking corrupt (operator report: whole UI
  broken, needed an MCU reset). A deliberate ~800ms hold means an accidental brush can't trigger it.
  **Additionally, the demo→normal direction is gated on a SETTLED STOP:** the hold only flips when
  `motorStatusFast.bits.motorState == MOTOR_STATE_STOPPED` AND it has been stopped ≥1s (tracked by
  `s_motor_stopped_since`, maintained in `tick_common_ui`). So to hand the wire structs back to the real UART
  path the operator must press STOP, let the motor settle ~1s, THEN hold the brand — no live handoff while the
  motor spins (user request). Turning demo ON (normal→demo) has no such gate.
  `main.c` reads `demo_sim_active` to gate its UART Rx handling in step with the switch. Don't revert this to
  `LV_EVENT_CLICKED`/a tap, and don't re-add `ext_click_area` (which made the target easier to hit-by-accident).
  `UI_DEMO_SIM` stays the compile gate (production firmware must NOT include the fake generator); the toggle
  is a within-demo-build convenience. **The STARTING/STOPPING state machine is
  LEVEL-driven, not edge-driven** (`sim_step` re-evaluates `motorCmd.bits.cmd` vs `motorState` every tick):
  START pressed during the ~1.1s STOPPING ramp re-arms STARTING instead of being swallowed. The old
  edge-detect (`s_lastCmd`) lost a press that arrived mid-transition, leaving the motor STOPPED with cmd=1 and
  no pending edge → the "STOP then START won't restart" bug. Don't reintroduce edge detection here. Demo
  telemetry is lively for the graphs: voltage sweeps the full 12–56 V, current waves ~5–18 A once running.
  **`sim_step` is FLOAT-FREE (integer / fixed-point) — no `sinf`, no `rand()`, no `<math.h>`/`<stdlib.h>`**
  (the old float version — 2×`sinf` + ~7×`rand()` + dozens of float mul/div per tick — was heavy on a no-FPU
  MCU). The two graph sweeps index a 32-entry `int16` sine LUT (`SIN32`, linearly interpolated via a
  fixed-point phase); the RPM chase and temps ease with integer division (the `+/-1` in the RPM ease is
  load-bearing — plain `d/16` truncates to 0 when `|d|<16` and would stall the speed a few short of the
  setpoint). No per-tick random jitter, so displayed digits don't churn (fewer redraws too). The
  slow-telemetry fields (`mtTemp`/`invTemp`/`efficiency`/`pwmDuty`) refresh at **~1 Hz** (a `s_slow_div`
  counter) to match the real `motorStatusSlow` cadence; `demo_sim_toggle` ON seeds the temps so Monitor isn't
  briefly 0 °C. Keep it integer — don't reintroduce float/`sinf`/`rand` here.
- **Type scale (2026 legibility pass)**: bumped from the original mono36/20/14/12 set to a 3-tier system —
  hero `ui_font_mono66` (digits-only — Dashboard RPM + Control SPEED/TORQUE, ~50% up from mono44), `ui_font_mono44` (digits-only, POSITION centre readout), value `ui_font_mono22` (card/readout values), and a single
  merged label tier `ui_font_mono20` that replaced the old 12/14 split (badges, section labels, everything
  else). **No custom UI text sits below 20px anymore** — a user complaint that the original sizes hurt HMI
  readability drove this; don't reintroduce anything smaller than mono20 without the same complaint being
  re-litigated. (The FontAwesome icon font that briefly followed the same bump was later removed entirely —
  all icons are custom images now, see the icon bullet above.) Built-in Montserrat also collapsed to a
  single enabled size, `lv_font_montserrat_20` (see the `LV_SYMBOL_*` gotcha right below) — `lv_conf.h`
  no longer enables 12/14/16 at all, only 20.
- **Dashboard gauge**: one 320×320 `ui_img_dashboard_gauge` ALPHA_4BIT face pre-renders the extended/faded
  reference arc, 0..200 labels, tick lane, inner ring and faint BLDC cutaway. Runtime LVGL widgets overlay only
  the flat-ended green speed arc, RPM number/unit and one CW/CCW direction icon. The gauge sits in a dedicated
  hero card above equal Run Time, Iq Current and Faults cards. Do not rebuild the static face from LVGL geometry
  or zoom the ALPHA image; regenerate it at native size with `tools/gen_dashboard_gauge.py`.
- **Fonts**: `fonts/ui_font_mono66|44|30|22|20.c` (JetBrains Mono Bold subsets — bold everywhere now, used for
  nearly every label in the app so text reads as clearly weighted rather than thin/faint; `mono44` and
  `mono30` are digits+punct only — `mono66` = Dashboard RPM + Control SPEED/TORQUE heroes, `mono44` = POSITION centre readout, `mono30` = Dashboard card values POWER/MOTOR
  TEMP; `mono22` full-ASCII readout values; `mono20` full-ASCII label floor). There is no icon font — icons
  are images (see the icon bullet above). All generated — never hand-edit; change `tools/build_assets.bat`
  and regenerate. Custom mono fonts are ASCII + `°` only — no `·`, `Ω`, arrows (so filled pills/labels can't
  show a `●` bullet either — the Dashboard state pill uses a small `lv_obj` dot child instead).
  - **No `LV_SYMBOL_*` glyphs are used anywhere anymore** — every button is plain mono text and every icon
    is a custom image (see the icon bullet). This sidesteps the old trap (`LV_SYMBOL_*` glyphs live only in
    Montserrat, so a mono label containing one renders tofu). If you ADD a button/label with an
    `LV_SYMBOL_*` in its text, it must be on `&lv_font_montserrat_20` — but prefer a `ui_icon_*` image + a
    plain mono label instead, to stay consistent. **The `lv_dropdown` arrow was the one built-in-symbol
    trap** (it draws `LV_SYMBOL_DOWN` in the widget's own font): solved by `lv_dropdown_set_symbol(dd,
    &ui_icon_chevron)` (a custom image, recolored via `LV_PART_INDICATOR` `img_recolor`) so the dropdown
    font can be `ui_font_mono20`. Any other widget that draws a built-in symbol internally (roller, spinbox,
    keyboard, message-box buttons) has the same trap — swap its symbol for an image the same way.
    `lv_font_montserrat_20` stays enabled in `lv_conf.h` only as LVGL's `LV_FONT_DEFAULT` theme fallback; no
    label sets it explicitly now.
- Colors: a **light theme** with a **monochrome-first semantic system**. It STARTED from Apple HIG tokens
  but was then **deliberately pushed off them for cheap IPS/TFT legibility** (a later hardware-driven user
  brief: raise contrast, avoid gray-on-gray, saturate accents ~10-20%, keep colors distinct after RGB565).
  So don't "restore Apple values" — the current values are the intended ones. Palette in `COLOR_*` defines
  in `screens.h` (page bg `0xE1E4EA` = a CLEAR light gray, NOT near-white — white surfaces must pop off it;
  white cards `0xFFFFFF`; **chrome `COLOR_TOP_BG` is white now too**, not page-colored; border `0xAEB4BE` =
  a visibly-darker-than-hairline gray so outlines read on a cheap panel; text ramp `0x141417`/`0x2E3138`/
  `0x4B4F57`/`0x6A6E77` — darker AND more separated in luminance so tiers don't blur; `COLOR_ACCENT` =
  `0x2C2E33` deep charcoal solid-fill) +
  `screens.c`: `COLOR_GAUGE` = Apple systemBlue `0x007AFF` (kept — already a bold saturated hero),
  `COLOR_OK` `0x188C34` (green, +~12% sat), `COLOR_DANGER` `0xD70015` (kept), `COLOR_WARN` `0xE24700`
  (brightened from a dark `0xC93400` so WARN orange is unmistakably distinct from DANGER red after RGB565 —
  they used to sit too close). Structural fills: `COLOR_PANEL_BG` `0xE7EAEF` is now used ONLY for the link
  pill on the white top bar (a light-gray chip on white); everything that sits on the gray PAGE (mode
  buttons, chart cards, dropdowns, secondary buttons) is white `COLOR_CARD_BG`, not page-gray — white-on-gray
  is the contrast rule. `COLOR_ACCENT_BG` `0xD5DBE6` (selection tint, deepened so it reads). Also
  `COLOR_BRAND_RED` (Hyphen Deux logo — a brand color, never re-themed), `COLOR_ACCENT_BG`/`COLOR_ACCENT_DIM`
  (neutral tints for selected items), `COLOR_PANEL_BG`/`COLOR_TEXT_DIM`. Semantics (deliberate, don't regress):
  - **Speed uses a GREEN band scheme; azure (`COLOR_GAUGE 0x007AFF`) is now only the VOLTAGE graph.**
    `band_color()` returns `COLOR_GAUGE_GREEN` (`0x14B85A`) / `COLOR_WARN` / `COLOR_DANGER` for the <60% / <85%
    / ≥85% speed bands — the Dashboard + Control gauge **value arc AND hub dot** both take the band color (they
    change together; the needle stays dark ink), and the Graphs SPEED series is the same green. Azure is used
    for exactly ONE spot now: the **Graphs VOLTAGE series** (that chart's range is 0–80 V). The POSITION ring's
    dot (`ctrl_pos_dot`) was moved OFF azure to charcoal `COLOR_ACCENT` (user request — it reads as a drag
    handle like the slider knobs). Don't make the gauge/speed anything but the green band scheme.
  - **`COLOR_ACCENT` (`0x2C2E33`, a deep charcoal) is a solid fill for primaries + active selections**: the
    Settings SAVE CONFIG primary button, the **active direction (CW/CCW) toggle**, the Control level slider's
    fill/knob border, and the active-tab underline. (Control's mode selector is a **dropdown** now, not a
    segmented control; its `CALIBRATE` is NOT solid accent — it's a white + accent-border/text **secondary**,
    beside the green START / red STOP, which use green/red not accent.) The direction-button active state used to be a
    light `COLOR_ACCENT_BG` tint but the finalized mockup put a solid dark-accent fill on it — matching the
    mockup won, don't revert to the tint. Rule of thumb: a two-state toggle / primary button may fill solid
    accent; a passive "this row is selected" highlight stays `COLOR_ACCENT_BG` tint + `COLOR_ACCENT_DIM` text.
  - **Data readouts are neutral ink** (`COLOR_TEXT_H`) — color on a number means something (see below), so
    it's not spent by default.
  - **Green (`COLOR_OK`) = healthy/running**: RUNNING, CONNECTED (incl. the Settings LINK row), good
    efficiency, the Diagnostics per-fault check marks + "ALL SYSTEMS NORMAL" banner, the Voltage graph
    series, and the START button (industrial START-green/STOP-red convention). (Monitor's thermal *bars*
    were removed — temp threshold feedback is now the value text color, not a green/amber/red bar.)
  - Amber = WARNING, red = FAULT, unchanged. Gauge band thresholds: azure <60%, amber <85%, red above.
  - **The text ramp has been deepened THREE times, each away from Apple toward more contrast.** Apple's
    secondaryLabel/tertiaryLabel tokens are tuned for San Francisco at native iOS sizes and, measured plain,
    fall short of WCAG AA (4.5:1) for small bold text on white (the original `COLOR_TEXT_L` `0x86868B` was
    only ~3.2:1). Pass 1 dropped each tier one notch; pass 2 (~×0.9 per channel) followed the move to a 20px
    type floor; pass 3 (the cheap-IPS/TFT brief) deepened them again AND widened the luminance gaps between
    tiers so H/M/L/VL stay visually distinct on a low-contrast panel. Current values: `TEXT_H 0x141417`,
    `TEXT_M 0x2E3138`, `TEXT_L 0x4B4F57`, `TEXT_VL 0x6A6E77`, `COLOR_TEXT_DIM 0x83878F` (screens.c),
    `COLOR_ACCENT 0x2C2E33`. Keep the 5-level order (H > M > L > VL > DIM) and the separation if you adjust
    further; never move any tier back toward the lighter literal Apple value — that faintness is exactly what
    all three passes fixed.
  The Dashboard face is an alpha-only image recolored by LVGL, so it does not bake the card background.
  The splash logo still bakes its background: the canonical prepared source
  `tools/assets/ui_logo.png` must match `create_splash()`'s bg color — check it on a palette pass, or a
  visible rectangle appears around the logo. `lv_theme_default_init` in `ui.c` passes `COLOR_ACCENT` as the
  theme primary (light flag `false`) so default widget states stay charcoal, not azure. Slider knobs are
  white with a charcoal border (invisible otherwise on light bg).
- `COLOR_TOP_BG` (top/tab bars) is **white now** (`0xFFFFFF`, == `COLOR_CARD_BG`). This REVERSES an earlier
  rule (chrome used to equal `COLOR_BG` because flat-white-on-white read as glaring) — the cheap-panel pass
  darkened the page to a clear gray, which makes white chrome crisp rather than glaring, so white chrome is
  now correct. Don't revert it to page-gray. The **splash** background, however, still uses `COLOR_BG` (the
  gray page color), NOT the white chrome — so `tools/assets/ui_logo.png` must be regenerated from the brand artwork and flattened to match
  `create_splash()` (currently `0xE1E4EA`), and `ui_image_logo.c` must be regenerated any time the splash bg changes, or a
  visible rectangle appears around the logo (its alpha is flattened onto that exact color at asset-build
  time). SPLASH_BG tracks the splash/page color, not the top bar.
- Corner radius is a deliberate, generous 10px on cards/panels/buttons/rows (Apple's rounded-rect look —
  bumped up from a flatter 4px). Status pills/badges use `LV_RADIUS_CIRCLE` (full capsule). Full-bleed
  structural elements (top bar, tab bar, individual tab buttons, the splash overlay) stay at radius 0 —
  don't round those, they're edge-to-edge.
- Operator-UX decisions (don't regress): Monitor shows 8 large-font values — CURRENT / DC BUS / POWER /
  EFFICIENCY (ELECTRICAL card) + MOTOR TEMP / INVERTER / SPEED / TORQUE (THERMAL & MOTION card, 4+4 balanced).
  TORQUE reuses the SPEED icon (`ui_icon_dash`) and reads `motor_torque_nm()`. This is still far below the
  original 17-value port that saturated the MCU and corrupted the UI on hardware (Id/Iq/per-phase were
  consolidated into the single `current` field the wire protocol actually has). The Monitor row's static look
  (icon recolor + name + value font/size/align) lives in **shared styles** `st_mon_icon/_lbl/_val`
  (`styles_ensure()`), and `tick_screen_monitor` builds each value string with **`mon_set()` (no per-tick
  `snprintf`** — `fmt_fixed` already formats the number with a light integer `%ld` core, `mon_set` just
  appends the unit). No "COM3" strings (PC port name).
  Dashboard cards show power + motor temp, not actual/target speed (those duplicate the gauge/setpoint).
  PWM duty + efficiency live on Monitor; Control was simplified to just direction + the active setpoint.
- `rules.md` (Vietnamese) is the original style guide. Still-load-bearing points: `lv_obj_set_pos()` is
  relative to the parent's *content area* (already inset by padding — `init_screen_bg()` sets `pad_top 116`
  for the 60px top bar + 56px tab bar), and slider knobs clip under `LV_SIZE_CONTENT` parents without
  `LV_OBJ_FLAG_OVERFLOW_VISIBLE`.
  Its old `loadScreen`/`auto_del` per-tab-screen design is superseded — don't reintroduce it. Rule #1
  survives: update `PLAN.md` when finishing work.
- Firmware integration contract (main loop calls `ui_init()` then `ui_tick()` + `lv_timer_handler()`;
  UART parser writes `ms`, never calls LVGL from ISRs) is documented in `PERF.md`.

## Gotchas

- **Near-white hex values can render with a faint magenta/blue tint under RGB565** (5-6-5 bits): a value
  like `0xFBFBFD` puts R and B right at their 5-bit ceiling while G is one step under its 6-bit ceiling, so
  after reconstructing to 8-bit R/B end up relatively boosted vs. G — a visible pink cast on a full-screen
  fill even though the hex looks like "basically white". Found via the splash screen background. Prefer
  pure `lv_color_white()` or values where channels land on clean quantization steps for large flat fills;
  the effect is imperceptible on small elements (borders, pills) so it's not a concern there.
- `tools/assets/ui_logo.png` is already flattened onto `create_splash()`'s background
  color. Regenerate it from the brand artwork against the new color, then run `tools\build_assets.bat`, if the splash background
  changes; otherwise the opaque INDEXED_8BIT image shows a visible rectangle.
- Generated font/image files include `"lvgl.h"` only when `LV_LVGL_H_INCLUDE_SIMPLE` is defined —
  sim_pc's CMake does this; firmware projects must too.
- `sim_pc/CMakeLists.txt` uses an explicit source manifest. Add every new firmware asset/source there and
  to `tools/export_mcu.bat`; re-run CMake configure after changing the manifest.
- PowerShell on this machine can't run the exe with `&&` chains; the Bash tool with the exported w64devkit
  PATH is the reliable path.
