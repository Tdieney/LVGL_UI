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
- [x] Torque-mode target migrated from torque to q-axis current Iq (mA on wire, A on UI)
- [x] Mode-specific limit inputs (`limitRaw`) for velocity and current limits

## Phase 9: Single-Screen SPA Hardening & Memory Footprint
- [x] Single resident content screen architecture (`lv_obj_clean` before rebuild)
- [x] Coalesced and rate-limited screen tab transitions from `ui_tick()`
- [x] Zero dynamic string allocation during refresh loops (`label_bind_buffer()`)
- [x] Static captions trỏ Flash memory (`lv_label_set_text_static()`)
- [x] Validated MCU memory profile: **42 KB LVGL Heap** + **16 KB Partial Draw Buffer** (`ui_mcu_profile.h`)

## Phase 10: Telemetry & Demo Integration
- [x] Strict separation between UART wire structs (`MotorCmd_t`, `MotorStatusFast_t`, `MotorStatusSlow_t`) and Demo telemetry
- [x] Long-press demo toggle (~800ms) with STOP-state interlock
- [x] Standalone integer-only math for demo telemetry (`sin_lut`, zero soft-float calls)
- [x] 120-second automated regression test suite (`tools/run_regression.bat`)

## Phase 11: Display-only Dashboard Redesign
- [x] Create four 800x480 display-only Dashboard concepts for review (`drafts/dashboard_display_only/`)
- [x] Create four additional display-only concepts covering operating point, phase balance, signal stack and condition rings
- [x] Regenerate all eight Dashboard concepts with only Speed, Iq, Irms, Voltage, Run Time, Direction, Power and Faults (`drafts/dashboard_display_only_v2/`)
- [x] Redesign eight glance-first Dashboard concepts with Speed as the sole hero and only Run Time, Direction and Faults as support (`drafts/dashboard_display_only_v3/`)
- [x] Select Dashboard 08 — Hero Tile after visual review
- [x] Implement Hero Tile without START/STOP, direction controls or speed setpoint widgets
- [x] Match Hero Tile to the selected mockup with a 270x292 pre-rendered motor cutaway and dedicated 120px/38px readout fonts
- [x] Use one telemetry-selected direction icon per Dashboard/Monitor view; keep the Monitor icon neutral black
- [x] Move Dashboard direction above the speed bar, replace its former card with Iq Current, and right-align Monitor direction with numeric values
- [x] Visually balance the Dashboard Run Time and Iq Current value baselines within their cards
- [x] Re-run six-tab screenshots, heap measurements and stress regression after implementation

## Phase 12: Protocol and Control Simplification
- [x] Rename `motor_comm_protocol.h` to `motor_comm.h` and update the MCU export manifest
- [x] Retire OPEN_LOOP while preserving SPEED=1, TORQUE=2 and POSITION=3 wire values
- [x] Preserve target and limit settings independently for each supported Control mode
- [x] Use Iq as the TORQUE-mode target and feedback; use current limit for POSITION
- [x] Reduce the Position ring diameter by about 10%
- [x] Remove Hall Sensor and Encoder rows from Diagnostics
- [x] Synchronize the Settings LINK row through `ui_motor_connected()`
- [x] Replace Monitor telemetry with Iq, Irms, Speed, Voltage, Run Time, Direction, Power and Faults
- [x] Create three lightweight/pre-render-friendly Monitor concepts (`drafts/monitor_redesign/`)
- [x] Implement Monitor concept 02 without topic headings or nominal-current captions
- [x] Reset the UI-local Run Time at each accepted START and clear it on device reboot

## Phase 13: RS-485, Control and Scale Alignment
- [x] Make Settings a full-width RS-485-only page; remove Interface and Motor Parameters
- [x] Remove CALIBRATE and let START/STOP share the full Control action row
- [x] Rebalance the Control Target group and switch mode layouts immediately without a link/status echo
- [x] Rename Graph telemetry to Iq and Irms with a fixed 0..10 A current scale
- [x] Apply a 200 RPM speed ceiling and a 3.0 A UI Iq/current ceiling
- [x] Add CONNECTED, NO RESPONSE and DISCONNECTED link states with distinct colors
- [x] Extend stress regression to cycle SPEED, TORQUE and POSITION Control layouts
- [x] Re-run six-tab screenshots, heap measurements, syntax checks and MCU export verification

## Phase 14: Product Limit and Settings Persistence Update
- [x] Revise the product ceiling to 200 RPM and 3.0 A across UI controls, bars and Demo telemetry
- [x] Keep Graph Iq/Irms telemetry scales at 0..10 A
- [x] Implement RESET as pending factory defaults (921600 / None / 1)
- [x] Implement SAVE feedback plus toolchain-independent target load/commit hooks
- [x] Load validated RS-485 configuration before creating persistent UI chrome
- [x] Equalize all Settings rows and remove the trailing LINK STATUS divider
- [x] Retire the RS-485 1.5-stop-bit option; retain only 1 and 2 stop bits
- [ ] Rebuild simulator, review screenshots and export MCU package (operator handoff)

## Phase 15: Dashboard Speed-Indicator Exploration
- [x] Produce six display-only Dashboard concepts centered on distinct speed indicators
- [x] Include an LVGL/NXP E-Bike-inspired arc concept adapted to the motor HMI
- [x] Keep the concepts implementable with static RGB565 assets and lightweight numeric/segment updates
- [x] Revise concept 04 by replacing its vertical bar with the reviewed E-Bike-style speed arc
- [x] Correct concept 04 to use a thick vertical arc spanning from below CTRL to the IQ CURRENT card
- [x] Refine concept 04 with edge-to-edge double-weight arc, dot ticks and an aligned speed stack
- [x] Create a dark demo-ready revision with a balanced speed hierarchy and static-asset-friendly depth
- [x] Produce a cohesive modern color-system mockup for all six current tabs (`drafts/modern_all_tabs_theme/`)
- [x] Replace the rejected neon theme with a restrained Warm Graphite/ivory six-tab concept (`drafts/modern_all_tabs_theme_v2/`)
- [x] Explore a third six-tab Swiss Technical / soft color-blocking system (`drafts/modern_all_tabs_theme_v3/`)
- [x] Implement the four Graph cards from the selected Precision Cockpit reference with stable series colors and one shared pre-rendered Alpha-4 top-edge mask
- [ ] Select one concept and implement it in LVGL after operator review

## Phase 16: Precision Cockpit UI/UX Review
- [x] Capture the current simulator in Demo RUN across all six tabs and record heap measurements
- [x] Review the current navigation, telemetry hierarchy, control affordances and semantic colors
- [x] Produce a cohesive six-tab Precision Cockpit redesign for operator review (`drafts/uiux_redesign_review/precision_cockpit/`)
- [x] Document implementation constraints for the validated 52 KB LVGL heap profile
- [ ] Select, revise or reject the Precision Cockpit direction before changing firmware UI source

## Phase 17: Icon Sidebar and Telemetry Layout
- [x] Replace the horizontal text tab bar with a persistent 72 px icon-only left sidebar
- [x] Reflow all six tabs into one shared 728x420 content frame
- [x] Replace the Dashboard motor bitmap/segmented bar with a centered display-only 270-degree speed gauge
- [x] Arrange Run Time, Iq Current and Faults as three equal Dashboard cards
- [x] Replace Monitor's hero/list composition with eight equal 4x2 telemetry cards and colored title accents
- [x] Resize the pre-rendered Graph accent mask for the narrower two-column card width
- [x] Refresh six documentation screenshots and the measured 52 KB heap profile
- [x] Pass syntax, position-math, 120-second virtual stress and MCU export manifest verification
- [x] Place the Dashboard gauge inside a dedicated hero card and pre-render its outer arc, speed labels, tick lane, inner ring and motor motif from telemetry-gauge concept 01
- [x] Remove the Dashboard SPEED caption while retaining RPM and the live direction icon
- [x] Regenerate all six sidebar navigation icons as crisp native 30x30 Alpha-4 assets
- [x] Enlarge the layered Dashboard gauge face to 320x320 and use flat-cut live speed-arc endpoints
- [x] Extend the gray reference arc beyond the live range with faded tails, enlarge the speed-scale numerals, and replace the center ornament with a detailed BLDC cutaway

## Phase 18: Workspace Cleanup
- [x] Keep the selected Dashboard gauge reference and current documentation screenshots
- [x] Remove superseded design reviews, raw framebuffer captures and temporary simulator builds
- [x] Remove unused motor/font assets and row icons from generators and the MCU export manifest
- [x] Preserve the offline LVGL source/build cache for deterministic simulator builds

## Phase 19: Developer Tooling
- [x] `tools/shoot_all.bat` — capture all six tabs to PNG + `heap_summary.txt` for visual review (see `tools/README.md`)
- [x] `sim_pc.exe --layout <tab>` — text widget-tree dump (class, effective visibility, coordinates, widget values) for headless layout debugging
- [x] Docs restructure: slim `README.md`, split developer docs by topic into `docs/` (`ARCHITECTURE.md`, `SCREENS.md`, `TELEMETRY.md`, `PERFORMANCE_MEMORY.md`, `BUILD_TEST.md`, index in `docs/README.md`)
- [x] Add `DEV_LOG.md` as the shared activity log for developers and AI agents

## Phase 20: Post-AI Repository Audit
- [x] Reconcile the reorganized documentation with the current source and measured behavior
- [x] Make STOP cancel an unacknowledged/offline START command and regression-test the race window
- [x] Remove the dynamic-label fallback that could retain a pointer to stack text
- [x] Add validated `--mode speed|torque|position` and duration handling to headless simulator review
- [x] Harden six-tab shot validation with heap-output and exact RGB565-size checks
- [x] Restore reproducible logo generation with a compact canonical source asset
- [x] Pass strict syntax, 120-second virtual stress, six-tab shots, asset check and MCU manifest verification

## Phase 21: Visual Consistency
- [x] Sync the Graphs page surface colors (page, cards, borders) to the shared semantic palette so it matches the other five tabs; keep grid, axis and series identity colors local
- [x] Re-run six-tab shots, heap measurements and the full regression after the change
- [x] Make the left navigation rail span the full 480 px screen height; shorten the top bar to cover only the content column right of the rail (content frame 728x420 unchanged)

## Phase 22: Typography & Control Polish
- [x] Add Segoe UI Semibold sans fonts (20/22 px) for captions and buttons; numeric readouts, units and technical values stay on the JetBrains Mono fonts
- [x] Replace the Control MODE dropdown with a three-button segmented control (SPEED/TORQUE/POSITION); rework the regression's mode check to assert per-mode layout content
- [x] Rebalance the Settings card with `LV_FLEX_ALIGN_SPACE_EVENLY` so the four config rows and buttons spread across the card height instead of bunching at the top
- [x] Re-run regression, six-tab shots and heap measurements (Control 73%, stress min-free 14.9 KB)

## Phase 23: Nav & Top Bar Modernization
- [x] Active tab highlight -> floating 44x44 rounded pill (COLOR_ACCENT_BG + charcoal icon) with rail `pad_row` breathing room
- [x] Top bar: drop the redundant "MOTOR" label; motor state becomes a tinted chip (bg+border follow state); RS-485 pill border follows the link state
- [x] Re-run regression, shots and heap measurements (chrome +~1.7 KB, stress min-free 13.2 KB)

## Phase 24: Chrome → Content Cohesion
- [x] Remove the rectangular top bar; the header becomes a transparent floating band (brand left, MOTOR state chip + RS-485 pill right) over the page background
- [x] Make the nav rail transparent (no border/fill); active tab = white 48x48 pill + charcoal icon, idle = transparent + gray icon
- [x] Bump the six sidebar icons from 30x30 to 36x36 (regenerated Alpha-4 assets), pill 44 -> 48
- [x] Re-run regression, six-tab shots and heap measurements (unchanged budget; stress min-free 13.2 KB)
- [x] Evaluate and then revert the dark-rail variant (charcoal + 20% white pill) — operator kept the transparent-rail white-pill treatment

## Phase 25: Shell Redesign Review — operator chốt hướng A
- [x] Thêm cơ chế `UI_SHELL_VARIANT` (0-3) vào `sim_pc/CMakeLists.txt` + `screens.{h,c}` — palette, header band, sidebar, active tab state; nội dung 6 tab không đụng
- [x] Build Release (LVGL v8.4.0) + chụp 6 tab cho 3 hướng: A = charcoal pill, B = azure pill + tên trang dưới brand, C = white pill + vạch azure → `docs/review/variant_{a,b,c}`
- [x] Syntax -Werror cả 4 variant + regression mặc định PASS; heap không đổi (Control 76%, stress min-free 13.2 KB)
- [x] Operator chọn **hướng A** ("thẻ nổi trên nền thoáng"): page 0xE9ECF1, viền 0xD6DAE1, pill link 0xEEF0F4, tab active = pill charcoal + icon trắng
- [x] Xóa toàn bộ scaffolding biến thể — hướng A thành thiết kế mặc định duy nhất
- [x] Regression đầy đủ PASS + refresh shots chính thức (`docs/screenshots`) + `export_mcu --verify` PASS + cập nhật `docs/SCREENS.md`

## Phase 26: Operator UX Review Pass
- [x] Header: tên trang hiện tại thay brand text; pill RS-485 theo link state (bỏ baud); icon Settings → bánh răng (regen asset)
- [x] CLEAR FAULTS → disabled "CLEAR UNAVAILABLE" (chờ wire command)
- [x] Control: hàng MODE có nhãn đầu card; FWD/REV icon + CW/CCW caption; disabled states thật (START/STOP/MODE/dir/slider theo trạng thái motor); khóa START khi FAULT; knob slider solid accent
- [x] Monitor: title neutral + vạch identity; "RMS CURRENT"; RUN TIME/DIRECTION nhẹ hơn
- [x] Graphs: title chuẩn hóa (time caption "-25 s / NOW" đã thêm rồi bỏ theo yêu cầu operator)
- [x] Diagnostics: grid 2 cột; blink giới hạn ~6 s cho fault mới
- [x] Settings: LOAD DEFAULTS; SAVE disabled thật khi sạch (bỏ label UNSAVED CHANGES theo operator); SAVED check / FAILED tự revert; safety interlock "STOP FIRST" khi motor đang chạy
- [x] Dashboard: stack MODE + TARGET hai cánh hero; card dưới cao hơn
- [x] Contrast: border 0xC2C8D1, gauge green 0x0B8F45
- [x] Regression + 6 shots + export verify PASS; docs memory cập nhật số đo mới (Control 79%, stress min-free 9.8 KB)

## Phase 27: Soft Color Zoning + Control Layout
- [x] Tint header Monitor/Graphs tăng một cấp (COLOR_HDR_*) cho RGB565; tint lớn Control giữ palette nhạt
- [x] Control: CTRL_MODE_W 360 / CTRL_RCOL_W 280 tách riêng; MODE hết bị ép
- [x] Feedback SPEED + IQ thành 2 tile ngang xếp dọc (full width, gap 12, caption trái, mono56 + unit phải, bar đáy, bỏ 2 spacer)
- [x] POSITION: command panel hẹp 280, ring area rộng cân bằng hơn
- [x] Regression + 6 shots + export verify PASS; heap Control 83%, stress min-free 8.0 KB (trên guardrail)

## Phase 28: Contrast Pass — Monitor/Control (chỉ màu)
- [x] Accent mới COLOR_METRIC_*: Speed #0057D9, Iq #006F63, RMS #5B21B6, Voltage #B45309, Direction #3949AB, Healthy #137A34, Neutral #566173
- [x] Monitor header tint mạnh hơn (COLOR_HDR_*): D1E2FF/CBEAE4/E1D6F5/FFDCBF/D7DDF4/D1EAD7/E0E4EA; FAULTS zero-state #137A34
- [x] Control tint nhẹ: Speed tile #E2EDFF, Iq tile #DDF1ED, command panel #EEF1F5, slider track #D5DCE7, fill slider theo control, POSITION dot #0057D9 + ring #B5C4DB
- [x] Regression + shots + export verify PASS; heap không đổi đáng kể

## Phase 29: Quantum Signal — Embedded World Showcase (Monitor + Control)
- [x] Hệ ba token INK/SIGNAL/TINT cho 7 metric (Speed/Iq/RMS/Voltage-Magenta/Direction/Healthy/Runtime)
- [x] Monitor: header tint + title INK + underline SIGNAL; Control: tile tint + bar SIGNAL + MODE #0057A8 + CW/CCW navy + slider theo SIGNAL
- [x] Regression + shots + export verify PASS; heap không đổi đáng kể; START offline giữ nguyên

## Phase 30: Quantum Signal — fix root-cause mapping + pixel verify
- [x] Thay switch(lv_color_to32) bằng metric_id_t enum + bảng metric_palette_t const (không thể lệch sau RGB565)
- [x] mon_metric_card nhận metric id; xóa các helper so màu + toàn bộ define màu metric cũ
- [x] Control tile: left border 4px SIGNAL để nhìn ra palette khi value = 0
- [x] Pixel-verify Monitor 8 card (band + underline) và Control border (telemetry 0 + demo RUN) — đúng màu từng metric
- [x] Regression + shots + export verify PASS

## Phase 31: Instrument Deck — Embedded World Showcase
- [x] Monitor reorder theo domain (Motion/Current/Electrical/System), SPEED đầu tiên
- [x] Monitor header navy #132238 đồng nhất + title/underline SIGNAL (bỏ pastel)
- [x] Control feedback tiles dark (#0D2946/#0C3031) + value trắng; command panel sáng
- [x] POSITION panel dark #0D2946 + ring #36536F; MODE active #0068B5
- [x] Pixel QA 8 card Monitor + Control (0 và POSITION); regression + shots + export PASS

## Phase 32: Graph header đồng bộ Monitor
- [x] Bỏ padding ở chart card parent để header navy chạm sát mép trên/trái/phải
- [x] Đồng bộ card radius 12, header height 46 và clip hai góc trên như Monitor
- [x] Chuyển horizontal/bottom padding xuống chart body để giữ khoảng thở của plot
- [x] Regression + 6 shots PASS; Graphs 64%, free 19,184 B, biggest 18,376 B, frag 5%

## Phase 33: Runtime/xSPI Profiling & Hot-path Optimization
- [x] Thêm `sim_pc --profile <tab>` đo pixel/s, refresh burst và flush callback sau settle
- [x] Thêm deterministic POSITION drag profile đi qua input driver thật
- [x] Tách target POSITION khỏi `lv_arc` display-only; không invalidate track 212px transparent
- [x] Giữ handle ở cadence input, giới hạn readout mono44 ở 10 Hz và force giá trị khi release
- [x] Đổi Graph history từ `memmove` 49 sample sang ring buffer O(1), giữ nguyên rolling SHIFT 2 Hz
- [x] Thêm regression guard cho POSITION drag và Graphs xSPI budget
- [x] Review raster hóa: giữ asset cho geometry phức tạp; không bake full-screen/content do Flash + dirty bbox
- [x] Khôi phục bước generate `ui_font_mono56.c` bị thiếu trong asset pipeline
- [x] Full syntax/stress/shots regression và cập nhật memory/profile docs

## Phase 34: Neutral POSITION + Diagnostics Shadow
- [x] Bỏ dark metric deck/viền SIGNAL khỏi POSITION; ring #B6C3D2, handle #17324F và ink text
- [x] Tách surface/shadow CLEAR UNAVAILABLE sang wrapper normal-state, giữ button con disabled thật
- [x] Chốt rasterization policy: hybrid theo measurement, không blanket-bake nav/header/shadow vì không giảm dirty xSPI và tốn Flash/alpha blend
- [x] Full syntax/mode/xSPI/stress/six-shot regression PASS; Diagnostics còn 24,296 B free, stress min-free 9,832 B

## Phase 35: Content-scoped Tab Switching
- [x] Đo baseline thật: mọi switch full-screen 384,000 px / 768,000 B / 48 flush bất kể màu nền
- [x] Thêm persistent transparent `ui_ContentHost` 712x414; tab chỉ clean/rebuild children trong content
- [x] POSITION ring + Iq Limit transparent, dùng nền trắng card cha; bỏ hai opaque fill
- [x] `ui_tabbar_set_active()` chỉ restyle pill cũ/mới thay vì cả sáu tab
- [x] Thêm `--profile-switch` và regression đủ vòng sáu tab, guard ≤340,000 px
- [x] Đo sau tối ưu 318,192..329,280 px / 42..43 flush (−14.25..17.14%); full regression PASS

## Phase 36: White Control Feedback + Instrument Headers
- [x] SPEED/IQ feedback bỏ full dark/tint body; dùng nền trắng card cha
- [x] Tái dùng title label làm header navy 46px + SIGNAL underline như Monitor/Graphs, không thêm object
- [x] Command column SPEED/TORQUE transparent/trắng đồng bộ POSITION
- [x] Value chuyển về ink/unit gray, inactive progress track sáng và cao 8px
- [x] Thêm steady xSPI guard riêng SPEED/TORQUE ≤10k px/s, burst ≤16k
- [x] Đo 5,928 px/s / 14,820 burst; full regression PASS, stress min-free 9,664 B

## Phase 37: Active Control + Single-Object Navigation + 42 KB Heap
- [x] Chỉ tạo Control subtree đang hoạt động: SPEED/TORQUE dùng chung LEVEL, POSITION tạo riêng; không giữ widget inactive dưới cờ hidden
- [x] LEVEL↔POSITION dùng same-tab rebuild deferred/coalesced từ `ui_tick()`; SPEED↔TORQUE cấu hình tree tại chỗ
- [x] Gom 6 button + 6 pill + 6 image của navigation thành một custom-drawn/click widget, tái dùng icon Alpha-4 const trong Flash
- [x] Thêm `--test-nav` đi qua pointer/event path thật cho đủ sáu hit-band và đưa vào regression
- [x] Đo heap 52 KB trước khi hạ cấu hình: navigation tiết kiệm khoảng 6.4 KB resident; Control active-only tiết kiệm 4.5 KB ở LEVEL và 11.3 KB ở POSITION
- [x] Hạ `UI_LVGL_HEAP_BYTES` 52 KB → **42 KB**, trả thêm đúng 10,240 B static SRAM; tổng heap + draw buffer còn 59,008 B
- [x] POC damage-map sinh từ screenshot bị loại vì một số switch tăng 345–379k px và có rủi ro ghost; production giữ 318,192..329,280 px/switch
- [x] Heap 42 KB stress 120s: min-free 10,360 B, min-biggest 10,216 B, frag-max 14%; nâng guard lên 8 KB / 8 KB
- [x] Full regression + six shots + MCU export verify PASS; cập nhật tài liệu memory/architecture/tooling

## Phase 38: Control Instrument Grouping
- [x] Bọc SPEED và IQ CURRENT bằng shared outline 1px bo góc trên chính container hiện có
- [x] Giữ body transparent/trắng, không thêm wrapper, shadow hay opaque fill
- [x] Xác nhận heap không đổi: 11,808 B free / 9,400 B biggest; SPEED/TORQUE 5,913 px/s, burst 14,784 px
- [x] Refresh ảnh Control SPEED/TORQUE và chạy regression

## Phase 39: Dashboard SPEED Arc Bands
- [x] Đổi arc bình thường từ green sang SPEED ink blue, dành green cho START/healthy semantics
- [x] Dùng cùng ngưỡng Control: blue <85%, amber ≥85%, red chỉ khi wire fault hoặc vượt 200 RPM
- [x] Cache band để không ghi style mỗi SLOW tick; arc value/dirty geometry giữ nguyên
- [x] Thêm `--test-dashboard-bands` cho 169/170/201 RPM, fault và recovery; full regression PASS

## Phase 40: Top-Chip Contrast + Command Surface Restore
- [x] Tăng contrast MOTOR state tint và RS-485 neutral background khoảng 10-20 RGB levels, thêm outline 1px
- [x] Khôi phục command surface `#EDF2F7` bên phải cho SPEED/TORQUE và POSITION theo Git-index reference
- [x] Giữ hai feedback instrument body trắng + outline; không phục hồi dark/tint full-body cũ
- [x] Xác nhận SPEED/TORQUE 5,913 px/s, POSITION drag 93,433 px/s; stress 10,344/10,200 B; full regression PASS

## Phase 41: Top Status Height Alignment
- [x] Đồng bộ MOTOR state chip `pad_ver` 3→5 với RS-485 pill
- [x] Xác nhận bounding height bằng nhau bằng simulator layout dump; regression PASS

## Phase 42: Two-Phase Tab Feedback + Dashboard SPEED Signal
- [x] Tách tab switch thành chrome refresh trước, content teardown/build ở `ui_tick()` kế tiếp
- [x] Giữ rapid-tap coalescing/throttle và same-tab Control rebuild; không clean widget trong callback
- [x] Cull năm nav cell ngoài `draw_ctx->clip_area` trong custom draw callback
- [x] Đổi Dashboard SPEED arc normal sang SIGNAL cyan `#00B7FF` đúng token của Control
- [x] Profile đủ 6 switch: chrome 16,632..18,552 px, content burst 301,560..310,728 px, đúng 2 frame
- [x] Full regression + six screenshots + MCU export verify PASS; stress 11,368/10,200 B, frag-max 14%

## Phase 43: Motor-control project close-out
- [x] Remove tracked design drafts and experimental mockups from `drafts/`
- [x] Remove local simulator builds, raw captures, temporary review files, and local IDE/AI settings
- [x] Keep only final source, reproducible asset inputs/tools, technical handoff docs, and validated screenshots
- [x] Run the complete regression suite on the final motor-control source before archiving the branch

## Backlog / Future Tasks
- [ ] Mở rộng shared `lv_style_t` chỉ khi profiling trên target cho thấy cần thêm heap; không giả định trước mức tiết kiệm
- [ ] Target-side UART driver integration (external to UI repo — refer to `PERF.md`)
- [ ] On-device parameter editing in Settings (spinbox/keypad integration if requested)
