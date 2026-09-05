# DEV_LOG — Nhật ký hoạt động

Nhật ký chung cho **mọi thay đổi vật chất** trong repo: code, asset, tooling, quyết định
thiết kế — ghi bởi developer hoặc AI (bất kể công cụ). Mục tiêu: người vào sau biết
"ai làm gì, tại sao, chạm file nào" mà không phải đoán.

Quy tắc:

- Mỗi mục ghi ở phía trên phần entry cùng ngày (mới nhất đầu tiên trong ngày, các ngày
  mới thêm vào đầu file).
- Ngắn gọn, đủ thông tin: **ngày · tác giả · phạm vi · tóm tắt · file chạm tới**.
- Nếu liên quan tính năng/kế hoạch: ghi kèm phase trong `PLAN.md`.
- Không thay thế `PLAN.md` (trạng thái tính năng) và không thay thế tài liệu kỹ thuật
  (trong `docs/`) — entry nên chỉ vào file tài liệu liên quan nếu cần.

---

## 2026-09-05

- **Repository · Motor-control project close-out (Phase 43)** — Removed the tracked
  design exploration directory and local generated artifacts/configuration while retaining
  final firmware UI source, reproducible asset tooling, technical handoff documentation,
  reference screenshots, and regression coverage. Added `.codex-tmp/` to the ignore rules.
  The complete simulator regression passed before final archival. File: `.gitignore`,
  `drafts/*` (removed), `PLAN.md`, `DEV_LOG.md`.

## 2026-08-18

- **UI · Graphs: Đồng bộ Thang đo RMS CURRENT Y-Axis sang Max 2.0 A** —
  Cập nhật đồ thị `RMS CURRENT` trên trang GRAPHS sang dải đo 0 .. 2.0 A (đồng bộ với `IQ CURRENT`):
  - Hợp nhất `#define GRAPH_CURRENT_MAX_A 2` và `#define GRAPH_CURRENT_MAX_TEXT "2"` trong `screens.c`.
  - Trục Y của `RMS CURRENT` hiển thị nhãn min `0` và max `2`.
  - Hàm `tick_screen_graphs()` chuẩn hóa `phaseCurrentRms` (mA) sang deciAmps (`irms_ma / 100`, dải 0..20)
    đưa vào chuỗi dữ liệu sóng và bộ nhớ đệm `g_hist`, đảm bảo độ phân giải mịn 20 bậc trên toàn dải 0..2.0 A.
  - Cập nhật chú thích trong `screens.h` phản ánh đúng thang đo 2.0 A.
  Full regression và six shots PASS 100%. File: `screens.h`, `screens.c`, `docs/screenshots/*`, `DEV_LOG.md`.

- **UI · Graphs: Đồng bộ Thang đo IQ CURRENT Y-Axis sang Max 2.0 A** —
  Cập nhật đồ thị `IQ CURRENT` trên trang GRAPHS:
  - Thêm `#define GRAPH_IQ_MAX_A 2` và `#define GRAPH_IQ_MAX_TEXT "2"` trong `screens.c`.
  - Trục Y của đồ thị chuyển sang nhãn `0` và `2` (thay vì 10).
  - Chuỗi dữ liệu cuộn thời gian thực nhận `iqCurrent` theo đơn vị deciAmps (0.1 A/LSB) với dải 0..20,
    giúp đường sóng có độ phân giải mịn 20 bậc trên toàn dải 0..2.0 A mà không bị thô/gãy khúc.
  Full regression và six shots PASS 100%. File: `screens.c`, `docs/screenshots/*`, `DEV_LOG.md`.

- **UI · Control: Tách biệt Giới hạn Đặt lệnh Setpoint (UI_CTRL_MAX_RPM 200 RPM)** —
  Phân tách kiến trúc giữa thang đo Telemetry/Bảo vệ (`UI_MAX_RPM = 250 RPM`) và
  dải trượt điều khiển lệnh thực tế của người vận hành (`UI_CTRL_MAX_RPM = 200 RPM`):
  - Thêm `#define UI_CTRL_MAX_RPM 200` vào `screens.h`.
  - Slider `Target` trong SPEED mode và slider `Speed Limit` trong TORQUE mode kẹp trần ở `0 .. 200 RPM`.
  - Giữ nguyên thang đo Dashboard (0..250 RPM) và Graphs Y-axis (0..250 RPM) tạo dải an toàn (Safety Headroom) 50 RPM;
    thanh LEVEL bar khi kéo tối đa 200 RPM đạt 80% (`200 / 250`), luôn giữ màu an toàn `#075FE8`.
  - Cập nhật simulator test suite (`sim_pc/main.c`) và kiểm thử hồi quy.
  Full regression và six shots PASS 100%. File: `screens.h`, `screens.c`, `sim_pc/main.c`,
  `docs/screenshots/*`, `DEV_LOG.md`.

- **UI · Speed & Current Limits: SPEED MAX 250 RPM, IQ CURRENT MAX 2.0 A** —
  Cập nhật toàn diện giới hạn tốc độ và dòng điện tối đa trên toàn bộ hệ thống:
  - `UI_MAX_RPM`: nâng từ 200 lên **250 RPM** (`UI_MAX_RPM_TEXT` = `"250"`).
  - `UI_MAX_CURRENT_MA`: giảm từ 3000 mA (3.0 A) xuống **2000 mA (2.0 A)**.
  - Tái tạo asset mặt đồng hồ Dashboard (`ui_img_dashboard_gauge.c` via `gen_dashboard_gauge.py`):
    vạch chia 0..250 RPM với 6 nhãn chính (0, 50, 100, 150, 200, 250) và 25 vạch chia tỉ lệ 10 RPM/vạch.
  - Cập nhật đồ thị GRAPHS: thang SPEED Y-axis từ 0..250 RPM.
  - Cập nhật Control sliders: dải điều khiển tốc độ 0..250 RPM, giới hạn dòng Iq 0..2.0 A.
  - Cập nhật level bars & utilization bands (ngưỡng cảnh báo 85% = 212 RPM).
  - Cập nhật demo simulation (`demo_sim.c`) và bộ kiểm thử hồi quy (`sim_pc/main.c`, `run_regression.ps1`).
  Full regression và six shots PASS 100%. File: `screens.h`, `screens.c`, `demo_sim.c`,
  `tools/gen_dashboard_gauge.py`, `ui_img_dashboard_gauge.c`, `sim_pc/main.c`, `docs/screenshots/*`, `DEV_LOG.md`.

- **UI · Ultra High-Contrast Ink Palette & Dashboard SPEED Arc Sync** —
  (1) Căn giữa tiêu đề 8 thông số trên MONITOR (`LV_ALIGN_TOP_MID, 0, 13` +
  `LV_TEXT_ALIGN_CENTER`) tạo bố cục đối xứng, cân đối hoàn hảo.
  (2) Nâng cấp bộ màu sang **Ultra High-Contrast Industrial Palette** (tương phản
  ≥ 5.5:1 đến 7.8:1 trên nền card trắng `#FFFFFF`), loại bỏ hoàn toàn hiện tượng
  chìm/mờ trên màn hình LCD RGB565 thực tế, đồng bộ toàn diện trên cả 4 màn hình
  (Dashboard, Monitor, Control, Graphs):
  - `SPEED`: Cobalt Electric Blue `#075FE8` (đồng bộ cả arc gauge Dashboard,
    header Control, header Monitor, trace/title Graphs).
  - `IQ CURRENT`: Deep Jade Teal `#00705E`
  - `VOLTAGE` & `POWER`: Deep Industrial Amber `#B84500`
  - `RMS CURRENT`: Imperial Purple `#591A8F`
  - `DIRECTION`: Deep Naval Indigo `#283593`
  - `FAULTS`: Deep Forest Emerald `#066839`
  - `RUN TIME`: Slate Charcoal `#3E4C59`
  Full regression và six shots PASS. File: `screens.c`, `sim_pc/main.c`,
  `docs/screenshots/*`, `DEV_LOG.md`.

- **UI · Monitor: khôi phục màu Quantum Signal cho 8 tiêu đề thông số** —
  Khôi phục màu chữ header của cả 8 card đo lường trên tab MONITOR theo bảng
  Quantum Signal palette (`SPEED` cyan `#00B7FF`, `IQ CURRENT` teal `#00D4B0`,
  `VOLTAGE` magenta `#F052A5`, `RUN TIME` steel `#A7B7C9`, `DIRECTION` indigo
  `#7D8BFF`, `RMS CURRENT` violet `#A875FF`, `POWER` magenta `#F052A5`, `FAULTS`
  green `#2FD66D`). Full regression và six shots PASS. File: `screens.c`,
  `docs/screenshots/*`, `DEV_LOG.md`.

- **UI · Control: khôi phục màu Quantum Signal cho SPEED và IQ CURRENT** —
  Đổi màu chữ tiêu đề "SPEED" và "IQ CURRENT" trên 2 tile feedback của Control
  từ neutral gray `COLOR_TEXT_L` sang màu Quantum Signal palette (`#00B7FF` cyan
  cho SPEED, `#00D4B0` teal cho IQ) theo chuẩn thiết kế trước đó trong DEV_LOG.
  Full regression và six shots PASS. File: `screens.c`, `docs/screenshots/*`, `DEV_LOG.md`.

- **UI · Graphs: bỏ nền navy header 4 đồ thị** — Xóa background navy #132238 và
  underline 3px khỏi header của 4 card đồ thị trong trang GRAPHS; chuyển title
  và live value sang dùng màu trực tiếp của từng series (SPEED blue #075FE8, IQ
  teal #00796D, VOLTAGE amber #C45A00, RMS CURRENT violet #6327BE) nổi bật trên
  nền card trắng. Giữ nguyên header height tier 46px để không làm thay đổi kích
  thước plot box và đảm bảo ngân sách xSPI redraw ≤ 360k px/s (đo 346,836 px/s).
  Graphs heap free tăng thêm +240 B (15,120 B free / 65% used). Full regression
  và six shots PASS. File: `screens.c`, `docs/screenshots/*`, `DEV_LOG.md`.

## 2026-08-17

- **UI/Perf · Two-phase tab feedback + Dashboard SPEED signal (Phase 42)** — Tách
  tab switch thành refresh chrome riêng (hai nav cell cũ/mới + page title), tick sau
  mới clean/build content; giữ coalesce/throttle và same-tab Control rebuild ngoài
  callback. Nav custom draw cull 5 cell ngoài clip. Dashboard arc normal dùng cùng
  SIGNAL cyan `#00B7FF` với Control, amber/danger giữ nguyên. Sáu switch có
  `frames=2`, chrome 16,632..18,552 px, content burst 301,560..310,728 px; total
  318,192..329,280 px không đổi. Full regression + six shots + MCU export verify
  PASS; stress 11,368 B free / 10,200 B biggest / 14% frag.
  File: `ui.{c,h}`, `screens.{c,h}`, `sim_pc/main.c`, `tools/{run_regression.ps1,
  README.md}`, `docs/{ARCHITECTURE,SCREENS,UI_COMPONENT_MAP,PERFORMANCE_MEMORY,
  MCU_MEMORY_PROFILE,BUILD_TEST}.md`, `docs/screenshots/*`, `PLAN.md`.

- **UI · Align top status chip heights (Phase 41)** — MOTOR state chip đổi vertical
  padding 3→5 px để cùng font/border/padding-height với RS-485 pill; không thêm object
  hay style property. Xác nhận bounding height bằng layout dump và regression.
  File: `screens.c`, `PLAN.md`.

- **UI/Perf · Top-chip contrast + command surface restore (Phase 40)** — Đối chiếu
  `control.png` hiện tại với bản Git index và khôi phục đúng surface `#EDF2F7` cho
  cột TARGET/LIMIT/direction ở LEVEL lẫn Iq Limit ở POSITION; không kéo lại dark
  feedback body cũ. MOTOR state tint và RS-485 neutral chip được darken 10-20 RGB
  levels + outline 1px để tách khỏi header trắng. Steady xSPI không đổi: LEVEL
  5,913 px/s, POSITION drag 93,433 px/s; Control LEVEL còn 11,776 B free, stress
  10,344 B free / 10,200 B biggest / 14% frag. Full regression + shots PASS.
  File: `screens.c`, `docs/{SCREENS,UI_COMPONENT_MAP,PERFORMANCE_MEMORY,
  MCU_MEMORY_PROFILE}.md`, `docs/screenshots/*`, `PLAN.md`.

- **UI/Perf · Dashboard SPEED arc bands (Phase 39)** — Arc live đổi normal green
  sang SPEED ink blue (`#0057A8`), ≥85% chuyển amber; red chỉ khi wire fault hoặc
  telemetry vượt hard limit 200 RPM. Cache `dash_speed_band` nên chỉ đổi style lúc
  qua band, không ghi màu mỗi SLOW tick; geometry/heap giữ nguyên. Thêm simulator
  regression `--test-dashboard-bands` drive 169/170/201 RPM, fault và recovery.
  File: `screens.{c,h}`, `sim_pc/main.c`, `tools/{run_regression.ps1,README.md}`,
  `docs/{SCREENS,UI_COMPONENT_MAP,BUILD_TEST}.md`, `PLAN.md`.

- **UI/Perf · Control instrument grouping (Phase 38)** — Thêm shared border-only
  style 1px bo 12px cho hai container SPEED/IQ CURRENT hiện có để body trắng không
  lẫn vào nền Control. Dùng `border_post` để header/bar full-width không che outline;
  không thêm wrapper, shadow hoặc opaque body fill. Heap giữ nguyên 11,808 B free /
  9,400 B biggest ở Control; metadata shared style resident tốn 72 B trên các tab nhẹ
  hơn để tránh tăng local style array ở màn hình chật nhất. SPEED/TORQUE đo 5,913 px/s,
  burst 14,784 px. Refresh đủ sáu ảnh + SPEED/TORQUE; regression PASS. File:
  `screens.c`, `docs/{SCREENS,UI_COMPONENT_MAP,
  PERFORMANCE_MEMORY}.md`, `docs/screenshots/{control,control_torque}.png`, `PLAN.md`.

- **Perf/UI · Active Control + single-object navigation + heap 42 KB (Phase 37)** —
  Control giờ chỉ tạo subtree đang dùng: SPEED/TORQUE chia sẻ LEVEL tree, POSITION
  tạo riêng; LEVEL↔POSITION rebuild same-tab deferred từ `ui_tick()`, không clean
  object trong callback. Rail navigation giữ nguyên hình ảnh nhưng gom 18 child
  button/pill/image thành một custom-drawn/click object dùng icon const trong Flash;
  thêm regression `--test-nav` qua pointer path thật. Ở heap 52 KB, hai thay đổi tăng
  headroom Control LEVEL từ 11,112 lên 22,048 B và POSITION lên 28,832 B; navigation
  riêng tiết kiệm khoảng 6.4 KB resident. Sau đó hạ heap 52→42 KB, trả thêm đúng
  10,240 B static SRAM: Control LEVEL còn 11,808 B free / 9,400 B biggest, POSITION
  18,592 / 16,312 B; stress 120s đạt min-free 10,360 B / min-biggest 10,216 B /
  frag-max 14%, guard nâng lên 8 KB/8 KB. POC damage-map pairwise từ screenshot bị
  loại và revert vì tăng một số switch lên 345–379k px, có rủi ro ghost; production
  giữ invalidation chuẩn 318,192..329,280 px/switch. File: `ui.{c,h}`, `screens.c`,
  `sim_pc/main.c`, `ui_mcu_profile.h`, `tools/{run_regression.ps1,export_mcu.bat,
  README.md}`, `docs/*.md`, `AGENTS.md`, `README.md`, `PLAN.md`.

- **UI/Perf · White Control feedback + instrument headers (Phase 36)** — SPEED/IQ
  bỏ full dark/tint body và command column xám; toàn bộ body dùng card trắng như
  POSITION. Tái dùng hai title label hiện có làm header navy 46px + SIGNAL underline
  đúng Monitor/Graphs, nên không thêm object; value về ink, unit gray, track sáng 8px.
  Đo xác nhận màu full cũ không resend cả tile: SPEED/TORQUE 5,936 → 5,928 px/s,
  burst 14,840 → 14,820; lợi ích chính là bớt opaque fill CPU + visual nhất quán.
  Regression thêm guard ≤10k/≤16k; full suite PASS, stress 9,664 B free / 9,520 B
  biggest / 15% frag. File: `screens.c`, `tools/run_regression.ps1`, `docs/*.md`,
  `docs/screenshots/*`, `PLAN.md`.

- **Perf/UI · Content-scoped tab switch + white POSITION (Phase 35)** — Profiler mới
  xác nhận `lv_obj_clean(ui_MainScreen)` cũ resend đúng 384,000 px / 768,000 B / 48
  flush cho mọi cặp tab; màu trắng/xám không đổi traffic RGB565. Thêm persistent
  transparent `ui_ContentHost` 712x414, chỉ clean/build content, và chỉ restyle hai
  nav pill cũ/mới. POSITION ring + Iq Limit transparent dùng card trắng, bỏ hai fill.
  Sáu switch còn 318,192..329,280 px / 42..43 calls (giảm 14.25..17.14%); regression
  guard ≤340k. Full syntax/modes/xSPI/stress/shots PASS; stress 9,720 B free / 9,576 B
  biggest / 15% frag. File: `ui.c`, `screens.{c,h}`, `sim_pc/main.c`,
  `tools/{run_regression.ps1,README.md}`, `docs/*.md`, `docs/screenshots/*`, `PLAN.md`.

- **UI · Neutral POSITION + Diagnostics shadow (Phase 34)** — Bỏ dark-blue metric
  deck và viền cyan khỏi rotary POSITION; hai cột dùng chung surface #EDF2F7, ring
  #B6C3D2, handle #17324F, readout ink. CLEAR UNAVAILABLE giờ có wrapper normal-state
  sở hữu surface/shared shadow và button con disabled trong suốt, tránh opacity
  disabled làm shadow biến mất. Chốt tiếp tục raster hybrid: không blanket-bake
  nav/header/shadow vì chrome không redraw khi settled, ảnh không giảm dirty xSPI và
  thêm Flash/alpha blend. Full regression PASS: POSITION 93,433 px/s, Diagnostics
  24,296 B free / 23,256 B biggest / 5% frag, stress min-free 9,832 B.
  File: `screens.c`, `docs/{SCREENS,UI_COMPONENT_MAP,MCU_MEMORY_PROFILE,PERFORMANCE_MEMORY}.md`,
  `docs/screenshots/*`, `PLAN.md`.

- **Perf · Runtime/xSPI profiling + POSITION hot-path (Phase 33)** — Thêm
  `sim_pc --profile <tab>` đếm pixel/s, refresh burst và callback flush sau settle;
  `--drag` chạy một vòng Control/POSITION qua input driver thật. Root cause: ring
  `lv_arc` chỉ có track tĩnh/indicator transparent nhưng mỗi mẫu touch vẫn ghi value,
  invalidate arc 212px; readout mono44 cũng redraw ~33 Hz. Tách target sang
  `ctrl_pos_raw`, giữ handle theo cadence input, cap số ở 10 Hz + force RELEASED:
  **274,280 → 93,433 pixel/s (-66%)**, max burst **51,076 → 10,006 px (-80%)**.
  Regression khóa ≤120k px/s / ≤15k burst; Graphs rolling khóa ≤360k / ≤50k.
  File: `screens.c`, `sim_pc/main.c`, `tools/run_regression.ps1`, `docs/*.md`,
  `tools/README.md`, `PLAN.md`.

- **Perf · Graph history O(1) + rasterization review (Phase 33)** — Thay
  `memmove` 49 sample mỗi lần history đầy bằng ring buffer head/tail O(1), giữ nguyên
  thứ tự replay và semantics chart SHIFT 2 Hz. Đo Graphs 337,684 pixel/s; không bake
  full tab vì RGB565 800×480 = 768 KB/ảnh và ảnh không tự thu nhỏ dirty bbox. Giữ
  raster cho geometry phức tạp (gauge/icon/logo), còn card/fill tĩnh tiếp tục dùng
  primitive/shared style. Audit cũng khôi phục lệnh generate `ui_font_mono56.c` bị
  thiếu dù source đang dùng font này. Full syntax + position + xSPI guard + stress 120s + six
  shots PASS; stress 9,816 B free / 9,672 B biggest / 15% frag, export manifest và
  asset check PASS. File: `screens.c`, `docs/PERFORMANCE_MEMORY.md`,
  `docs/MCU_MEMORY_PROFILE.md`, `tools/build_assets.bat`, `PLAN.md`.

## 2026-08-16

- **UI · Graph header flush đúng cấu trúc Monitor (Phase 32)** — Root cause của
  header Graph vẫn giống một box inset là `pad_hor=10` và top padding nằm trên
  chart card cha dù comment ghi full-width. Đưa toàn bộ card padding về 0, chuyển
  horizontal/bottom padding xuống chart body; đồng bộ radius **12** và header
  navy #132238 cao **46px** với Monitor. Không thêm widget, plot vẫn đủ diện tích.
  Regression + 6 shots PASS; Graphs 64%, free 19,184 B, biggest 18,376 B, frag 5%;
  refresh bảng heap hiện hành và stress 9,784/9,648 B. File: `screens.c`,
  `docs/SCREENS.md`, `docs/UI_COMPONENT_MAP.md`, `docs/MCU_MEMORY_PROFILE.md`,
  `docs/PERFORMANCE_MEMORY.md`, `docs/screenshots/*`, `PLAN.md`.

## 2026-08-15

- **UI · Monitor: header band bo tròn góc** — Header navy của Monitor card giờ có
  radius 12 theo card (trước vuông góc chồng lên card bo tròn). Regression PASS,
  shots refresh. File: `screens.c`, `docs/screenshots/*`.

- **Perf · P0 shared card shadow + POC pre-render topbar** — (1) Card shadow chuyển
  thành shared style `st_card_shadow` trong `styles_ensure()` (10px/ofs 3/0x0D0F14/
  LV_OPA_20); `style_card_shadow()` giờ chỉ `lv_obj_add_style` — bỏ 4 local style
  prop x ~20 card mỗi rebuild, xóa 3 dòng `shadow_width 0` local chặn shared style
  (btn_clear/set_btn_save/btn_def). Kết quả đo: **stress min-free 8.8 → 9.9 KB**,
  **Control 83% → 79%** (11.4 KB free). (2) POC pre-render header band: asset
  `ui_img_topbar` (placeholder 712x58 ALPHA_4BIT full-rect) + `LV_IMG_DECLARE` +
  CMake/export manifest; `create_common_ui` có nhánh `UI_TOPBAR_IMG_POC` (mặc định
  OFF — fallback lv_obj giữ nguyên hành vi) dùng lv_img recolor làm band, dynamic
  children (title/chip/pill) giữ nguyên trên top. TODO: thay asset thật rồi bật
  define. Regression PASS, shots refresh, export --verify + git diff --check PASS.
  File: `screens.c`, `images.h`, `ui_img_topbar.c`, `sim_pc/CMakeLists.txt`,
  `tools/export_mcu.bat`, `docs/screenshots/*`.
- **UI · Graph header đúng style Monitor** — Làm lại band header 4 chart đúng
  chuẩn Monitor: band navy #132238 **full-width sát mép card** (clip_corner để
  bo đúng 2 góc trên), **title + value dùng SIGNAL color** của metric tương ứng
  (không còn màu series cũ), **underline 3px SIGNAL** thẳng ở đáy band.
  Regression PASS, shots refresh. File: `screens.c`, `docs/screenshots/*`.
- **UI · Diag bỏ divider cuối + Graph header navy** — (1) FAULT REGISTER: bỏ vạch
  kẻ dưới hàng cuối (RS-485 LINK). (2) Header 4 chart Graphs trở lại band nền
  **navy #132238 bo 8** theo style Monitor (title/value giữ màu series).
  Regression PASS, shots refresh. File: `screens.c`, `docs/screenshots/*`.
- **UI · Settings row 68px + regen splash logo** — (1) Hàng RS-485 64 → **68px**
  (dropdown → divider ~12px). (2) Splash: asset `ui_logo.png` còn flatten trên
  màu page CŨ (0xE1E4EA) trong khi splash overlay dùng COLOR_BG hiện tại
  (0xE9ECF1) → lệch nền. Remap 33,928 px nền cũ (và halo <=8 đơn vị) sang
  0xE9ECF1 trong canonical `tools/assets/ui_logo.png` + regenerate
  `ui_image_logo.c`. Regression PASS, shots refresh. File: `screens.c`,
  `tools/assets/ui_logo.png`, `ui_image_logo.c`, `docs/screenshots/*`.
- **UI · Settings: tăng khoảng cách dropdown → divider** — Hàng RS-485 56 → **64px**
  (dropdown 44px → ~10px thoáng cả trên lẫn dưới, hết cảm giác khung dính vạch kẻ).
  Regression PASS, shots refresh. File: `screens.c`, `docs/screenshots/*`.
- **UI · Graphs header phẳng + Diagnostics 5 khung dọc + Settings padding** —
  (1) Graphs: bỏ hẳn band màu trên header 4 chart (title/value giữ màu series,
  không chip). (2) Diagnostics: redesign fault register thành **5 khung chữ nhật
  dọc** full-width (1 cột grid, mỗi fault 1 hàng icon + tên + check/x, divider
  đáy). (3) Settings: pad dọc card 14 → 20px để khoảng cách trên/dưới các vạch
  phân cách đều hơn. Regression PASS, shots refresh, export --verify + git diff
  --check PASS. File: `screens.c`, `docs/screenshots/*`.
- **UI · Control: tile caption/value tách dòng + direction icon-only + POSITION gap** —
  (1) POSITION: gap "Iq Limit" → slider 11 → **18px** (bằng rcol). (2) SPEED/TORQUE
  tile: caption "SPEED"/"IQ CURRENT" tách dòng riêng **sát cạnh trên** (pad_top 8),
  số + unit thành dòng riêng bên dưới (SPACE_BETWEEN 3 con: caption / value / bar).
  (3) Nút direction bỏ chữ CW/CCW — **chỉ icon căn giữa nút** (hết lệch trái + clip
  icon); tick bỏ recolor text. Regression PASS, shots refresh, export --verify +
  git diff --check PASS. File: `screens.c`, `docs/screenshots/*`.
- **UI · Polish theo yêu cầu operator (Monitor/Control/Diag/Settings)** —
  (1) Monitor: header navy chỉ bo **2 góc trên** (dùng `lv_style_set_clip_corner`
  trên card + band vuông — không thêm object). (2) Control: caption SPEED/IQ
  đẩy lên (pad_top 14→10); MODE active + CW/CCW active đổi sang **COLOR_ACCENT
  (charcoal)** — cùng màu pill active của navigation; gap title→slider trong
  command panel 14→18. (3) Bỏ caption "TARGET ANGLE" ở POSITION. (4)
  Diagnostics: nút CLEAR ra khỏi card lỗi, nằm dưới cùng trang + shadow.
  (5) Settings: SAVE/LOAD DEFAULTS ra khỏi card (card 398→342), dưới cùng +
  shadow. Regression PASS, shots refresh, export --verify + git diff --check
  PASS. File: `screens.c`, `docs/SCREENS.md`, `docs/screenshots/*`.
- **UI · Bar utilization band mới — hết "giả lỗi" khi RUN** — Ngưỡng cũ (<60%
  signal, 60-85% amber, ≥85% red) làm 166 RPM hiện cam và Iq 3.0 A hiện đỏ khi
  motor đang RUNNING bình thường. Ngưỡng mới: **<85% giữ signal cyan/teal,
  85-100% amber; RED chỉ khi wire fault ≠ 0 hoặc telemetry vượt hard limit**
  (rpm > 200, iq > 3.0 A). Chạm configured limit (100%) chỉ amber. Regression
  PASS, shots refresh. File: `screens.c`, `docs/SCREENS.md`, `docs/screenshots/*`.

- **UI · Instrument Deck — Embedded World Showcase (Monitor + Control)** — Đổi
  visual language, không chỉ vài mã hex: (1) **Monitor reorder theo domain**: Motion
  (SPEED/DIRECTION), Current (IQ/RMS), Electrical (VOLTAGE/POWER), System (RUN
  TIME/FAULTS) — SPEED thành card đầu tiên. (2) **Header navy đồng nhất #132238**
  (bỏ pastel rainbow), title = SIGNAL color, underline 3px SIGNAL; body trắng,
  value ink; RUN TIME value #4D6078, icon DIRECTION #3546A8, FAULTS zero #08703A.
  (3) **Control dark instrument tiles**: SPEED bg #0D2946 + border/caption/bar
  #00B7FF (track #284760), IQ bg #0C3031 + #00D4B0 (track #244946), value trắng
  #F7FAFF, unit #B8C5D6; POSITION panel #0D2946, ring track #36536F, dot/caption
  #00B7FF. Command panel vẫn sáng #EDF2F7 (dark = feedback, light = command).
  MODE active #0068B5; CW/CCW giữ navy. Signal palette nâng (SPEED 00B7FF, IQ
  00D4B0, RMS A875FF, Energy F052A5, Direction 7D8BFF, Health 2FD66D, Runtime
  A7B7C9) qua bảng metric_palette — không thêm widget/font/asset, không gradient/
  blending, START offline giữ nguyên. **Pixel QA**: 8 header navy #10203A đồng
  nhất + underline signal từng metric đúng màu; Control border cyan/teal rõ cả
  khi telemetry=0 và POSITION. Regression PASS, shots + export --verify + git
  diff --check PASS. File: `screens.c`, `docs/screenshots/*`, `PLAN.md`.

- **Fix · Quantum Signal: mapping bằng enum + bảng palette (root cause)** — Helper
  `switch (lv_color_to32(accent))` so RGB888 literal không bao giờ match vì
  `lv_color_hex()` đã quantize RGB565 → mọi header/underline rơi về neutral.
  Thay bằng **`metric_id_t` enum + `metric_palette_t` bảng const** (`ink/signal/
  monitor_tint/control_tint`, `LV_COLOR_MAKE`); `mon_metric_card()` nhận metric id
  thay vì màu; xóa hẳn `metric_tint/metric_tint_strong/metric_signal` + mọi
  `COLOR_METRIC_*/COLOR_SIG_*/COLOR_HDR_*/COLOR_TINT_*`. Control tile thêm **left
  border 4px SIGNAL** (SPEED #00A8FF, IQ #00C9A7, POSITION #00A8FF) để palette
  hiện rõ cả khi bar value = 0. Graphs band dùng palette theo metric id (fix cùng
  lỗi). **Pixel-verify**: IQ band #C5EFE6/underline #00CAA5, RMS #E6D7FF/#9C69FF,
  SPEED #CEEBFF/#00AAFF, VOLTAGE+POWER #F7D2E6/#F74DA5 (magenta), DIR #D6DFFF/
  #6B7DFF (indigo), FAULTS #CEEBD6/#21CA6B (green), RUN #DEE3EF/#7B96AD — không
  còn fallback neutral; Control border cyan/teal thấy rõ cả telemetry=0 lẫn
  demo RUN. Regression PASS, shots + export --verify + git diff --check PASS.
  File: `screens.c`, `docs/screenshots/*`, `PLAN.md`.

- **UI · Quantum Signal palette (Monitor + Control)** — Palette ba token INK/
  SIGNAL/TINT kiểu Embedded World: Speed Ink #0057A8 / Signal #00A8FF; Iq
  #006E61/#00C9A7; RMS #5B2CAE/#9B6BFF; Voltage/Power **Plasma Magenta**
  #A91E68/#F04FA3 (thay amber); Direction #3546A8/#6F7DFF; Healthy #08703A/
  #23C969; Runtime #4D6078/#7F95AD. Monitor: header = Monitor tint (C9E8FF/
  C5EEE7/E1D6FA/F5D2E5/D7DEFA/CCEBD7/D9E3ED), title = INK, underline 3px =
  SIGNAL (helper `metric_signal()`), value chính giữ #0B1220, unit #59677A.
  Control: tile Speed #DDEFFF + bar SIGNAL #00A8FF (track #C1DBF2), tile Iq
  #DDF3EE + #00C9A7 (track #BEDFD8); POSITION dot #00A8FF + ring track #AFC4E1;
  command panel #EDF2F7; slider track #CED8E5, knob navy #17324F, fill theo
  SIGNAL của control; MODE active #0057A8 (inactive border #B6C3D2); CW/CCW
  active #17324F. Giữ nguyên START/STOP, disabled, typography, layout, protocol,
  telemetry, **START khi RS-485 disconnected**. Không thêm widget/font/asset.
  Regression PASS (heap không đổi đáng kể), shots + export --verify + git diff
  --check PASS. File: `screens.c`, `docs/screenshots/*`, `PLAN.md`.

- **UI · Contrast pass Monitor/Control (chỉ màu)** — Tăng contrast cảm nhận
  10-20% chỉ trên metric card Monitor + feedback/command panel Control, giữ
  light industrial, không đụng chrome/Dashboard/Graphs/Diagnostics/Settings,
  không đổi layout/font/widget/hành vi. **Accent mới**: Speed #0057D9, Iq
  #006F63, RMS #5B21B6, Voltage/Power #B45309, Direction #3949AB, Healthy
  #137A34, Neutral #566173 (`COLOR_METRIC_*` + `COLOR_METRIC_HEALTH` mới).
  **Monitor header tint** (mạnh hơn ~15-20%): #D1E2FF/#CBEAE4/#E1D6F5/#FFDCBF/
  #D7DDF4/#D1EAD7/#E0E4EA (`COLOR_HDR_*`). FAULTS card + zero-state value dùng
  #137A34 (danger thật vẫn COLOR_DANGER); RUN TIME value = #566173.
  **Control** (tint nhẹ ~10-12%): tile Speed #E2EDFF, tile Iq #DDF1ED, command
  panel #EEF1F5, slider track #D5DCE7, fill slider theo control nó sửa (TARGET
  = màu mode chính, LIMIT = màu phụ, Iq Limit POSITION = teal), POSITION dot
  #0057D9 + ring track #B5C4DB. START/STOP/MODE/disabled giữ nguyên; START khi
  RS-485 disconnected KHÔNG đổi (regression PASS). Heap không đổi đáng kể
  (stress min-free ~8.1 KB). File: `screens.c`, `docs/screenshots/*`,
  `PLAN.md`.

- **UI · Monitor: DIRECTION indigo + value/icon giữ màu cũ** — DIRECTION đổi
  accent/title/header tint sang **indigo #3F51B5 / #E1E6F6**; RUN TIME giữ gray
  #667085 cho accent/title/tint. **Value RUN TIME và icon direction giữ màu cũ**
  (COLOR_TEXT_M) theo yêu cầu operator. Regression PASS, shots refresh. File:
  `screens.c`, `docs/screenshots/*`.

- **UI · Zoning polish + Control layout mới** — (1) Header tint Monitor/Graphs tăng
  một cấp (`COLOR_HDR_*`: DCE9FF/D7EFEA/E9E0F7/FFE5D0/DDF0E2/E7EAEE) để sống sót
  RGB565; vùng tint lớn của Control giữ palette nhạt. (2) Control: tách
  **CTRL_MODE_W 360** (segmented MODE) khỏi **CTRL_RCOL_W 280** (command panel);
  garea chuyển COLUMN — **SPEED tile trên, IQ tile dưới**, full width bằng nhau,
  gap 12, caption trái + giá trị **mono56** + unit phải (baseline), bar sát đáy
  (SPACE_BETWEEN, bỏ 2 spacer → net +2 object); POSITION ring area rộng hơn, panel
  hẹp 280. Không đụng protocol/telemetry/START offline. Regression PASS — heap:
  Control 83% (9.5 KB free, frag 25%), stress min-free **8.0 KB / 8.0 KB** (vẫn
  trên guardrail 6/4 KB); shots + export --verify + asset check + git diff --check
  PASS. File: `screens.c`, `docs/SCREENS.md`, `docs/UI_COMPONENT_MAP.md`,
  `docs/MCU_MEMORY_PROFILE.md`, `docs/PERFORMANCE_MEMORY.md`, `docs/screenshots/*`.

- **UI · Soft color zoning** — Triển khai hướng phân vùng màu pastel của operator:
  palette tint cố định RGB565 (`COLOR_TINT_*` 0xEAF2FF/E7F5F2/F1ECFA/FFF1E6/
  EAF6ED/F0F2F5) + identity `COLOR_METRIC_*` (0x075FE8/00796D/6327BE/C45A00/667085)
  + helper `metric_tint()`, không opacity blending. **Monitor**: vạch 6px → header
  band 46px tint + underline accent 3px, title màu accent (tái dùng object `rule`).
  **Graphs**: head thành band tint 30px + value chip trắng bo tròn, plot nền
  F8FAFC (hết transparent), grid D8DEE7, trace 3px. **Control**: CTRL_RCOL_W
  280→360 (MODE hết bị ép), scol/tcol thành 2 tile tint xanh/teal bo 12 + gap 12,
  xóa 2 divider, số SPEED/IQ mono66→mono44, bar bình thường blue/teal (warn/danger
  mới amber/red), rcol/pcol thành command panel nền F3F5F8 bo 12 pad 14 (bỏ
  pad_top 58), POSITION parea nền tint xanh + ring track xanh xám B9C5D8. Dọn typo
  còn sót (confirmed/stacked/repeated...). Regression PASS (stress 8.8 KB min-free,
  Control 81%/23% frag), shots refresh, export --verify PASS. File: `screens.c`,
  `docs/MCU_MEMORY_PROFILE.md`, `docs/PERFORMANCE_MEMORY.md`, `docs/screenshots/*`.

- **Fix · Post-review pass (P0/P1)** — Xử lý toàn bộ góp ý review: (P0-1) Cache
  disabled-state của Control (`ctrl_last_start_ok/stop_ok/locked`) chuyển từ static
  local sang screen state + reset 0xFF trong `create_screen_control()` — hết bug nút
  nhìn sai trạng thái sau khi rời/quay lại tab. (P0-2) `settings_sync_controls()` gọi
  ngay khi build Settings — SAVE disabled đúng ngay frame đầu khi cấu hình sạch.
  (P0-3) Safety interlock: SAVE chỉ commit khi motor STOPPED + không lệnh treo, nếu
  không feedback **"STOP FIRST"** (warn). (P1) build_sim.ps1: thêm
  `FETCHCONTENT_FULLY_DISCONNECTED=ON` khi đã có cache LVGL + build target `sim_pc`
  (bỏ lvgl_examples/demos). Dọn toàn bộ dấu vết s→e còn sót (`tab_pills`,
  `ctrl_mode_btns`, `Buttons/Captions`...) + mojibake UTF-8. Fix blank line cuối
  `ui_font_mono30/38.c`. CLAUDE.md: motor_comm_protocol.h → motor_comm.h. Docs đồng
  bộ (SCREENS/BUILD_TEST/PLAN/COMPONENT_MAP/ARCHITECTURE) + cập nhật bảng heap mới
  (Control 81%/22% frag, stress 9.0 KB/15% frag). Visual: "SPEED LIM" → "Speed Limit",
  caption "TARGET ANGLE" + cột Iq Limit căn giữa dọc ở POSITION, pad_hor cho nút MODE;
  regression assert cập nhật theo tên mới. Full regression PASS (bao gồm build qua
  build_sim.ps1 mới). File: `screens.c`, `tools/build_sim.ps1`,
  `tools/run_regression.ps1`, `CLAUDE.md`, `docs/*.md`, `fonts/ui_font_mono30.c`,
  `fonts/ui_font_mono38.c`, `docs/screenshots/*`.

## 2026-08-14

- **Khôi phục · screens.c bị hỏng do lệnh replace PowerShell sai** — Trong lúc sửa
  Settings, một lệnh `String.Replace` PowerShell vô tình thay **mọi chữ 's' thường
  thành 'e'** toàn file (sai cú pháp mảng). Đã khôi phục lossless bằng: (1) khớp
  từng dòng với bản git index (2481/3110 dòng khôi phục tự động), (2) sửa 629 dòng
  đã edit trong session bằng từ điển token + các pass thay thế có chủ đích, (3)
  dùng trình biên dịch + regression để bắt nốt phần sót, (4) sửa mojibake UTF-8
  cho ký tự đặc biệt (— → • ° ✓). Verify: build sạch, full regression PASS (syntax
  2 chế độ -Werror, position, Control 3 modes, stress 120s, 6 shots), export
  --verify PASS. Cơ chế SAVE (disabled khi sạch, không có label UNSAVED) hoạt
  động đúng. File: `screens.c`, `docs/screenshots/*`.

- **UI · Bỏ cơ chế UNSAVED CHANGES ở Settings** — Operator không cần dirty
  tracking: xóa label "UNSAVED CHANGES", snapshot `s_rs485_saved`,
  `rs485_config_dirty()` + `settings_sync_controls()`; SAVE luôn khả dụng, giữ
  feedback SAVED/FAILED (check icon + revert 1.5s). Callback dropdown/reset giờ
  gọi `settings_show_feedback(0)`. Cập nhật docs. Regression PASS (Settings heap
  58 → 57%), shots refresh. File: `screens.c`, `docs/SCREENS.md`,
  `docs/UI_COMPONENT_MAP.md`, `docs/screenshots/*`.

- **UI · Icon direction Monitor +10** — Icon direction Monitor chốt offset **+10**.
  Regression PASS, shots refresh. File: `screens.c`, `docs/screenshots/*`.

- **UI · Icon direction Monitor +12** — Icon direction Monitor chốt offset **+12**.
  Regression PASS, shots refresh. File: `screens.c`, `docs/screenshots/*`.

- **UI · Icon direction Monitor → +14 + dọn comment lịch sử** — Icon direction
  Monitor chốt offset **+14**; dọn các comment kiểu "was X / X -> Y" trong
  `screens.c` + `gen_dashboard_gauge.py` (lịch sử đã có DEV_LOG). Regression PASS,
  shots refresh. File: `screens.c`, `tools/gen_dashboard_gauge.py`,
  `docs/screenshots/*`.

- **UI · Chi tiết: RS-485 22px, Dashboard card + Monitor direction** — (1) Chữ
  "RS-485" header mono20 → **mono22**. (2) Dashboard: giá trị 3 card dưới đẩy xuống
  thêm **4px** (pad_row 8 → 12). (3) Monitor: icon direction dịch xuống **6px**
  (+18 → +24) ngang hàng các con số. Regression PASS, shots refresh. File:
  `screens.c`, `docs/screenshots/*`.

- **UI · Monitor: số chính về mono44, RUN TIME giữ 30** — 6 giá trị (IQ/RMS/SPEED/
  VOLTAGE/POWER/FAULTS) lên **mono44**; RUN TIME giữ mono30 (8 ký tự không vừa 44).
  Regression PASS, shots refresh. File: `screens.c`, `docs/screenshots/*`.

- **UI · Arc speed 16 → 8px** — Vòng arc sống Dashboard giảm độ dày 16 → **8px**
  theo yêu cầu operator. Regression PASS, shots refresh. File: `screens.c`,
  `docs/screenshots/*`.

- **UI · Arc ngoài gauge +4 đơn vị** — Outer reference arc radius 142 → **146**
  trong `gen_dashboard_gauge.py` + regenerate `ui_img_dashboard_gauge.c`. Lưu ý:
  đáy arc giờ bị clip thêm ~4px bởi card 300px (mặt số 320 đặt lệch +12). Regression
  PASS, shots refresh. File: `tools/gen_dashboard_gauge.py`, `ui_img_dashboard_gauge.c`,
  `docs/screenshots/*`.

- **UI · Gauge label xích ra + doc component map + icon dir nudge** — (1) Số 150/200
  trên mặt gauge dính vạch chia: label radius 108 → **114** trong `gen_dashboard_gauge.py`
  + regenerate `ui_img_dashboard_gauge.c`. (2) Icon direction Dashboard +116 → **+112**.
  (3) Tạo **`docs/UI_COMPONENT_MAP.md`**: bảng quản lý từng component (chrome + 6 tab)
  kèm cột "Code hay Asset tĩnh" + nơi sửa; thêm vào index `docs/README.md`.
  Regression + shots + export --verify PASS. File: `tools/gen_dashboard_gauge.py`,
  `ui_img_dashboard_gauge.c`, `screens.c`, `docs/UI_COMPONENT_MAP.md`,
  `docs/README.md`, `docs/screenshots/*`.

- **UI · Dashboard: RPM unit 22px + icon direction xuống 20** — Chữ "RPM" tăng
  mono20 → **mono22**; icon direction dịch xuống **20px** (+96 → +116). Regression
  PASS, shots refresh. File: `screens.c`, `docs/screenshots/*`.

- **UI · Dashboard: RPM unit + icon direction xuống** — Chữ "RPM" dịch xuống **8px**
  (offset +58 → +66), icon direction dịch xuống **8px** (+88 → +96). Regression
  PASS, shots refresh. File: `screens.c`, `docs/screenshots/*`.

- **UI · Pill RS-485 gọn + số RPM xuống thêm** — (1) Header: pill chỉ còn dot trạng
  thái + chữ cố định "RS-485" (bỏ ONLINE/OFFLINE text + bỏ buffer top_pill_text).
  (2) Dashboard: số RPM dịch xuống thêm **4px** (+12 → +16, dưới tâm hình học của
  vòng 1 chút vì arc 270° hở đáy khiến số trông cao), đơn vị RPM theo xuống (+50 →
  +58). Regression PASS, shots refresh. File: `screens.c`, `docs/screenshots/*`.

- **UI · Fix dấu ':' RUN TIME Monitor + cân Dashboard card** — (1) Dấu ':' mất ở
  RUN TIME Monitor: font mono30 cũ chỉ có digits — regenerate thêm dấu ':' (0x3A)
  + cập nhật build_assets.bat. (2) Dashboard: giá trị 3 card dưới đẩy xuống **4px**
  (pad_row 4 → 8) cho cân đối. Regression PASS, shots refresh. File: `screens.c`,
  `fonts/ui_font_mono30.c`, `tools/build_assets.bat`, `docs/screenshots/*`.

- **UI · Dashboard RPM vào tâm vòng + fix dấu chấm IQ + Monitor một cỡ số** —
  (1) Dashboard: số RPM dịch xuống **12px** (offset 0 → +12) nằm giữa vòng gauge,
  đơn vị RPM dịch xuống theo (38 → 50). (2) Fix dấu '.' vỡ ở giá trị IQ (font
  mono38 trước chỉ có digits+colon): regenerate mono38 thêm `% , - .` (cập nhật
  build_assets.bat). (3) Monitor: mọi giá trị về **mono30** duy nhất (bỏ 44/38/22
  riêng lẻ), label value rộng 142 → 150 cho vừa "00:00:00", caption "H:M:S" →
  **"HH:MM:SS"**. Regression PASS, shots refresh. File: `screens.c`,
  `fonts/ui_font_mono38.c`, `tools/build_assets.bat`, `docs/screenshots/*`.

- **UI · Dashboard: RPM 66 + căn tâm + card cùng cỡ số** — (1) Số RPM tăng
  mono56 → **mono66** (cao hơn ~10px), màu giữ `COLOR_TEXT_H` — cùng màu số SPEED
  ở Control. (2) Số RPM đặt **chính giữa khung hero** (offset +5 → 0, dịch lên
  5px); vòng gauge dịch xuống **7px** (offset +5 → +12) nên số nằm trên tâm vòng
  một chút. (3) 3 card dưới cùng cỡ số **mono38**: RUN TIME tăng từ 30 (+8px), IQ
  giảm từ 44 (−6px), FAULTS giữ nguyên. Regression PASS, shots refresh. File:
  `screens.c`, `docs/screenshots/*`.

- **UI · Monitor bar/title lên trên + shadow nút/graph** — (1) Monitor: vạch màu
  dịch lên 4px (y 16→12) và dày 4→**6px** (radius 3), title dịch lên 4px (y 30→26).
  (2) Shadow: nút START/STOP Control + nút SAVE/LOAD DEFAULTS Settings + 4 card
  Graphs — nguyên nhân shadow "mất" là các hàng flex cha clip con (overflow
  hidden): thêm `OVERFLOW_VISIBLE` cho actions row, btn_row Settings, row1/row2 +
  main_cont Graphs, root Control/Diagnostics. Regression PASS, shots refresh.
  File: `screens.c`, `docs/screenshots/*`.

- **UI · Cân khoảng cách icon nav** — Operator phát hiện pill trên/dưới cùng không
  cách mép rail đúng 8px (~5px do flex chia 464/6). Chuyển 6 nút sang vị trí tuyệt
  đối `TAB_BTN_Y` {8,85,162,239,316,392}: pill đầu cách mép trên **đúng 8px**, pill
  cuối cách mép dưới **đúng 8px**, gap giữa 13/12px (464 không chia hết — flex cũ
  dồn lệch 4px xuống đáy). Regression PASS, shots refresh. File: `screens.c`,
  `docs/screenshots/*`.

- **UI · Pill + icon navigation phóng theo rail** — Theo góp ý operator (tăng width
  rail phải tăng cả khung icon): pill 56/bo 14 → **64/bo 16** (inset 8px = UI_GAP
  trong rail 80), 6 icon nav regen 40 → **44px**. Regression PASS, shots refresh.
  File: `screens.c`, `tools/gen_icons.py`, `ui_icon_{dash,mon,ctrl,graph,diag,set}.c`,
  `docs/screenshots/*`.

- **UI · Navigation bar rộng 80px** — `UI_SIDEBAR_W` 72 → **80** theo yêu cầu
  operator. Content co theo: `UI_CONTENT_W` 720 → 712, refit card: Dashboard hero
  696 + 3 card 226 (x=8/242/476), Monitor card 168 (x=8/184/360/536), Settings
  card 696; header band 696 rộng tại x=96. Regression PASS, shots refresh. File:
  `screens.c`, `docs/screenshots/*`.

- **UI · Navigation bar kiểu card nổi** — Rail trái chuyển từ cột trắng full-height
  sang **panel nổi**: 72x464 tại (8,8), bo tròn 4 góc (radius 14), shadow phẳng như
  card; cách mép trên/trái/dưới đúng `UI_GAP` (8px). Hệ quả: `UI_CONTENT_W` 728 →
  720, content pad_left 72 → 80 (card cách rail 8px), header band x 80 → 88.
  Các kích thước card không đổi (vẫn 704 hiệu dụng). Regression PASS, shots
  refresh. File: `screens.c`, `docs/screenshots/*`.

- **UI · Bỏ border card + shadow phẳng** — Operator muốn bỏ viền + đổ bóng: tất cả
  **bề mặt** (header band, hero/3 card Dashboard, 8 card Monitor, card Control, 4
  card Graphs, banner + card Diagnostics, card Settings, state chip, pill RS-485)
  bỏ border 1px và nhận `style_card_shadow()`: shadow phẳng LVGL (width 10, ofs_y 3,
  đen 20% opa) — **không gradient** nên panel RGB565 rẻ không bị banding; chỉ vẽ lúc
  build, tick không đụng viền shadow. **Giữ border chức năng** trên nút bấm
  (START/STOP/dir/mode/SAVE/DEFAULTS), dropdown, divider hàng — bỏ các border này
  sẽ mất affordance. Regression PASS (stress min-free 9.6 KB, heap ~+0.2 KB),
  shots refresh. File: `screens.c`, `docs/screenshots/*`.

- **UI · Header 58px** — `UI_HEADER_BAND_H` 60 → **58** (band 712x58 tại (80,8);
  `UI_HEADER_H` 66, content 414). Refit: Dashboard hero 300 (3 card y=316),
  Monitor card 195 (hàng 2 y=211), Settings card 398. Regression PASS, shots
  refresh. File: `screens.c`, `docs/screenshots/*`.

- **UI · UI_GAP 8 + header 60** — Operator chốt hệ spacing mới: `UI_GAP` 12 → **8**,
  `UI_HEADER_BAND_H` 64 → **60**. `UI_HEADER_H` = 68, content 412. Refit: Dashboard
  hero 298 + 3 card 232 (y=314, gap 8), Monitor card 172x194 (2 hàng y=8/210),
  Settings card 396, header band 712x60 tại (80,8). Mọi kẻ hở giờ 8px, viền 1px.
  Regression PASS, shots refresh. File: `screens.c`, `docs/screenshots/*`.

- **UI · Hệ spacing thống nhất `UI_GAP` = 12px** — Operator yêu cầu 1 giá trị chung
  cho mọi kẻ hở/đường viền: thêm `UI_GAP` 12 + `UI_HEADER_BAND_H` 64; header band
  inset 12px 3 cạnh; `UI_HEADER_H` 72 → 76 (kẻ hở dưới band = 12, không còn 8px);
  refit tab: Dashboard hero 278 + 3 card 226 (gap 12, y=302), Monitor card 167x184
  (2 hàng y 12/208, gap 12), Settings card 380, Control/Graphs/Diagnostics dùng
  UI_GAP cho pad (Graphs pad_ver 11→12, Diagnostics pad_row 14→12). Đường viền giữ
  đều **1px**. Regression PASS, shots refresh. File: `screens.c`,
  `docs/ARCHITECTURE.md`, `docs/screenshots/*`.

- **UI · Fix chồng lấn header 64px + refit các tab** — Header band 64px (y=12..76)
  che mất 4px đỉnh card đầu tiên → đẩy vùng nội dung xuống: `UI_HEADER_H` 60 →
  **72** (`UI_CONTENT_H` 420 → 408), card giờ cách đáy header 8px. Refit các tab
  dùng chiều cao cố định: Dashboard hero 294 → 282 (3 card 90 giữ nguyên, kẻ hở
  12 đều), Monitor card 194 → 188 (2 hàng y 84/280, đáy 12), Settings card 396 →
  384; Control/Graphs/Diagnostics tự co theo flex. Regression PASS, shots
  refresh. File: `screens.c`, `docs/screenshots/*`.

- **UI · Header band 64px** — Chiều cao header band 68 → **64px** (điều chỉnh nhỏ
  theo review operator). Regression PASS, shots refresh. File: `screens.c`,
  `docs/screenshots/*`.

- **UI · Header band 68px** — Chiều cao header band 52 → **68px** theo yêu cầu
  operator (y=12; cạnh dưới giờ chạm card đầu tiên — cả hai đều trắng nên đọc như
  một khối liền). Regression PASS, shots refresh. File: `screens.c`,
  `docs/screenshots/*`.

- **UI · Header band nâng height** — Tăng chiều cao header band 48 → **52px** theo
  yêu cầu operator (vẫn đặt y=12, kẻ hở dưới còn 8px). Regression PASS, shots
  refresh. File: `screens.c`, `docs/screenshots/*`.

- **UI · Header band hạ + giảm chiều cao** — Header band đẩy xuống 12px (y=12) và
  giảm 60 → **48px** để phía trên có kẻ hở xám 12px như 3 cạnh còn lại — chrome
  trắng giờ "nổi" đều quanh (khung nội dung 60..480 không đổi). Regression PASS,
  shots refresh. File: `screens.c`, `docs/screenshots/*`.

- **UI · Top bar inset + đều khoảng cách Dashboard** — Header band cũng thu vào
  12px 2 bên (84..788, w 704) thẳng cột với content. Dashboard: 3 card dưới hạ
  96 → **90px** để các kẻ hở đều nhau hết 12px (trên hero, hero↔cards, dưới cùng —
  trước đáy chỉ còn 6px). Regression PASS, shots refresh. File: `screens.c`,
  `docs/screenshots/*`.

- **UI · Thu width content — trả khoảng cách 2 bên** — Operator muốn component
  bên phải (ngoài navigation) có khoảng cách 2 bên: revert phần mở rộng flush 0px,
  content 6 tab về margin ngang **12px** đồng nhất (card hiệu dụng 704px); header
  band giữ nguyên flush 72..800 (khung trắng chữ L), brand/chips căn lề 12 khớp
  content. Regression PASS, shots refresh. File: `screens.c`, `docs/screenshots/*`.

- **UI · Chrome trắng frameless** — Operator yêu cầu bọc navigation + top giống card
  trắng không border: header band (72..800, 60px) và rail trái (0..72, full-height)
  chuyển từ trong suốt sang **trắng `COLOR_CARD_BG` không viền**, nối thành khung
  trắng chữ L quanh vùng nội dung; pill active charcoal vẫn nổi trên nền trắng.
  Regression PASS, shots refresh. File: `screens.c`, `docs/SCREENS.md`,
  `docs/screenshots/*`.

- **UI · Mở rộng trang + cân đối khoảng cách pill navigation** — Content 6 tab bỏ
  margin ngang (12/14 → 0): card giờ chạy sát mép rail (x=72) tới sát mép phải màn
  (x=800), hiệu dụng 704 → **728px**. Kết quả: khoảng cách mép trái → pill đen = 8px,
  pill đen → card = 8px — đối xứng (trước là 8 vs 20px lệch mắt). Card resize theo:
  Dashboard hero 728 + 3 card 236; Monitor 4 cột 176 (gap 8); Settings card 728;
  Control/Graphs/Diagnostics pad_hor 0; header brand/chips căn lề 8. Regression PASS,
  shots refresh. File: `screens.c`, `docs/screenshots/*`.

- **UI · Bỏ thanh màu đỉnh card Graphs** — Xóa accent mask Alpha-4 `ui_img_graph_accent`
  khỏi 4 card đồ thị theo yêu cầu operator; identity series còn lại ở title/value/trace.
  Dọn asset chết hoàn chỉnh: xóa `ui_img_graph_accent.c` + `tools/gen_graph_accent.py`,
  cập nhật `images.h`, `sim_pc/CMakeLists.txt`, `tools/export_mcu.bat`,
  `tools/build_assets.bat`, `tools/README.md`. Regression PASS, shots refresh,
  export --verify + asset check PASS. File: `screens.c`, `images.h`,
  `sim_pc/CMakeLists.txt`, `tools/export_mcu.bat`, `tools/build_assets.bat`,
  `tools/README.md`, `docs/SCREENS.md`, `docs/screenshots/*` (xóa 2 file asset).

- **UI · RPM mono56 + bỏ time caption Graphs** — (1) Dashboard: RPM hạ 44 → **56px**
  (sinh font mới `ui_font_mono56.c` — digits/punct only, 44.9 KB Flash; khai báo
  `ui_fonts.h` + `sim_pc/CMakeLists.txt`). (2) Graphs: bỏ caption "-25 s / NOW" theo
  yêu cầu operator (Graphs heap về 64%). Regression PASS, shots refresh. File:
  `fonts/ui_font_mono56.c`, `fonts/ui_fonts.h`, `sim_pc/CMakeLists.txt`, `screens.c`,
  `docs/SCREENS.md`, `docs/screenshots/*`.

- **UI · Chi tiết dashboard/monitor + tên trang** — (1) Header: `OVERVIEW` →
  `DASHBOARD`. (2) Monitor: title căn giữa card, vạch identity màu kéo rộng 42 →
  **142px căn giữa** trải sang 2 mép. (3) Dashboard: số RPM về **chính giữa vòng
  gauge** (offset (0,5) cùng mặt số/arc) và hạ font mono66 → **mono44**. Regression
  PASS, shots refresh. File: `screens.c`, `docs/SCREENS.md`, `docs/screenshots/*`.

- **UI · Navigation pill to hơn** — Operator muốn khối đen của tab active nổi bật
  hơn: pill 48x48/bo 12 → **56x56/bo 14**, 6 icon nav regen 36 → **40px** cho cân
  đối (`gen_icons.py` SIZES + `png2lvgl`). Regression PASS (heap không đổi), shots
  refresh. File: `screens.c`, `tools/gen_icons.py`, `ui_icon_{dash,mon,ctrl,graph,diag,set}.c`,
  `docs/SCREENS.md`, `docs/screenshots/*`.

- **UX · Operator UX review pass (7.5/10 → hành động)** — Triển khai toàn bộ nhóm
  vấn đề từ review vỏ + hành vi (nội dung telemetry không đổi): (1) **Header**: tên
  trang hiện tại thay brand text "MOTOR CONTROL HMI", pill "RS-485 ONLINE/OFFLINE/NO
  RESPONSE" (bỏ baud khỏi header); icon Settings đổi từ hình "target" sang **bánh
  răng** (`gen_icons.py` + regen asset). (2) **CLEAR FAULTS** → disabled +
  "CLEAR UNAVAILABLE" (hết nút active mà bấm không làm gì). (3) **Control**: hàng
  MODE có nhãn ở đầu card; FWD/REV = icon + caption **CW/CCW**; disabled states thật
  (STOP mờ khi dừng, START mờ khi chạy, FAULT khóa START cả UI lẫn `action_motor_start`,
  khóa MODE/dir/slider lúc STARTING/STOPPING); knob slider solid accent (hết nhìn như
  radio dot). (4) **Monitor**: title neutral, chỉ vạch identity giữ màu; "IRMS" →
  "RMS CURRENT"; RUN TIME/DIRECTION nhạt hơn. (5) **Graphs**: time context "-25 s" /
  "NOW"; title chuẩn hóa IQ CURRENT/RMS CURRENT. (6) **Diagnostics**: grid 2 cột
  (hết khoảng trống hàng cuối); blink chỉ ~6 s cho fault mới rồi đứng đỏ. (7)
  **Settings**: RESET → "LOAD DEFAULTS", dirty tracking + "UNSAVED CHANGES", SAVE
  disabled thật khi sạch, SAVED + icon check / FAILED tự revert 1.5 s. (8) **Dashboard**:
  stack MODE (trái) + TARGET (phải) lấp hai cánh hero, card dưới 90→96px. (9) Contrast:
  border 0xC2C8D1, gauge green 0x0B8F45 (~4.2:1). Regression đầy đủ PASS + 6 shots
  refresh + export --verify PASS. Heap dịch chuyển theo widget mới: Control 79%
  (11.2 KB free, frag 21%), stress min-free 9.8 KB / frag-max 14% — còn trên guardrail
  6/4 KB; docs memory đã cập nhật số mới. File: `screens.c`, `screens.h`, `actions.c`,
  `tools/gen_icons.py`, `ui_icon_set.c`, `docs/SCREENS.md`, `docs/MCU_MEMORY_PROFILE.md`,
  `docs/PERFORMANCE_MEMORY.md`, `docs/screenshots/*`, `PLAN.md`.

- **UI · Shell redesign — chốt hướng A làm mặc định** — Operator chọn hướng A
  ("thẻ nổi trên nền thoáng") sau khi xem 3 biến thể. Xóa toàn bộ scaffolding
  `UI_SHELL_VARIANT`: palette mới thành giá trị chính thức (`COLOR_BG` 0xE9ECF1,
  `COLOR_BORDER` 0xD6DAE1, `COLOR_PANEL_BG` 0xEEF0F4), tab active = pill charcoal
  đặc + icon trắng. Regression đầy đủ PASS (heap không đổi: Control 76%, stress
  min-free 13.2 KB), refresh 6 shots chính thức + heap_summary, `export_mcu
  --verify` PASS. Dọn thư mục review `docs/review/`. File: `screens.h`,
  `screens.c`, `sim_pc/CMakeLists.txt`, `docs/SCREENS.md`, `docs/screenshots/*`,
  `PLAN.md`.

- **UI · Shell redesign review — 3 biến thể vỏ ngoài** — Thêm cơ chế
  `UI_SHELL_VARIANT` (CMake cache var, 0 = thiết kế hiện tại) chạy 3 hướng mới
  song song, chỉ đụng vỏ ngoài (palette nền/viền, header band, sidebar, trạng
  thái tab active) — nội dung 6 tab giữ nguyên: **A** = nền/viền sáng thoáng
  hơn + tab active pill charcoal + icon trắng; **B** = như A + pill azure +
  tên trang đặt dưới brand trên header band; **C** = palette gần như cũ + pill
  trắng + vạch dọc azure cạnh trái. Build Release LVGL v8.4.0 + chụp 6 tab cả
  3 biến thể vào `docs/review/variant_{a,b,c}` cho operator xem. Syntax
  -Werror 4 variant PASS; regression mặc định PASS, heap không đổi (Control
  76%, stress min-free 13.2 KB). Sự cố phụ trong lúc làm: cache LVGL dưới
  `sim_pc/build/_deps` bị hủy khi reconfigure lúc mất mạng — đã khôi phục
  bằng re-fetch v8.4.0 sạch sau khi mạng ổn. File: `sim_pc/CMakeLists.txt`,
  `screens.h`, `screens.c`, `docs/review/*`, `PLAN.md`.

- **UI · Revert sidebar về transparent rail (Option 1)** — Đã thử Option 2 (dark rail
  charcoal + pill trắng 20%) rồi revert theo quyết định của operator: giữ transparent
  rail + white pill (active = pill trắng `COLOR_CARD_BG` + icon charcoal, idle = trong
  suốt + icon xám). Bỏ define `COLOR_RAIL_ICON`. Heap trở về đúng số Option 1
  (stress min-free 13.2 KB). Regression + shots PASS. File: `screens.c`,
  `docs/SCREENS.md`, `docs/screenshots/*`, `PLAN.md`.

- **UI · Sidebar dark rail (Option 2)** — Đổi rail sang variant tương phản: nền
  charcoal `COLOR_ACCENT` full-height, icon idle màu xám sáng `COLOR_RAIL_ICON`
  (0x9AA1AC), active = pill trắng 20% (`LV_OPA_20` — LVGL v8 không có LV_OPA_15) + icon
  trắng. Header band + nội dung giữ nguyên. Heap không đổi (stress min-free 13.2 KB).
  Regression + shots PASS. File: `screens.c`, `docs/SCREENS.md`, `docs/screenshots/*`,
  `PLAN.md`.

- **UI · Chrome hòa vào content (bỏ top bar cứng + sidebar trong suốt)** —
  (1) Top bar thành **header band trong suốt**: logo + "MOTOR CONTROL HMI" trái,
  state chip + pill RS-485 phải, nổi trực tiếp trên nền page (không strip trắng,
  không border). (2) Nav rail **trong suốt full-height**, bỏ border/nền; active tab =
  pill trắng 48x48 + icon charcoal, idle = trong suốt + icon xám — cùng họ với card.
  (3) 6 icon sidebar regen 30x30 → **36x36** (`gen_icons.py` SIZES). Khung nội dung
  giữ nguyên 728x420 (reserve 60px) nên 6 tab không sửa. Heap gần như không đổi
  (Control 76%, stress min-free 13.2 KB — frag 3%). Regression + shots PASS.
  File: `screens.c`, `tools/gen_icons.py`, `ui_icon_{dash,mon,ctrl,graph,diag,set}.c`,
  `images.h`, `docs/SCREENS.md`, `docs/MCU_MEMORY_PROFILE.md`,
  `docs/PERFORMANCE_MEMORY.md`, `docs/screenshots/*`, `PLAN.md`.

- **UI · Nav bar + top bar hiện đại hóa** — (1) Active tab đổi từ vạch 4px phải sang
  **pill nổi 44x44 bo 12px** (`COLOR_ACCENT_BG` + icon charcoal), rail thêm `pad_row 6`.
  (2) Top bar bỏ nhãn "MOTOR" thừa; trạng thái thành **chip nền tint** theo state
  (xanh/cam/đỏ/xám — cùng họ tint banner Diagnostics); viền pill RS-485 đổi màu theo
  link state. Note: `lv_color_hex()` không phải hằng số compile-time nên bảng chip
  dùng hàm helper thay vì const array. Chrome tốn thêm ~1.7 KB heap (Control 76%,
  stress min-free 13.2 KB — còn xa guardrail). Regression + shots PASS.
  File: `screens.c`, `docs/SCREENS.md`, `docs/MCU_MEMORY_PROFILE.md`,
  `docs/PERFORMANCE_MEMORY.md`, `docs/screenshots/*`, `PLAN.md`.

- **UI · Typography + Control + Settings polish** — (1) Thêm font sans caption/button
  `ui_font_sans20/22` (Segoe UI Semibold, vendor `tools/ttf/SegoeUISemibold.ttf`),
  áp cho title, section label, card title, button, fault name, chart title — mọi giá trị
  số/unit giữ mono. (2) Thay dropdown MODE bằng segmented control 3 nút; regression
  "Control headless modes" đổi sang assert nội dung layout theo mode. (3) Settings dùng
  `SPACE_EVENLY` dàn đều hàng theo chiều cao card, bỏ spacer. Heap: Control 71→73%
  (14.9 KB free), stress min-free 14.9 KB — còn xa guardrail. Regression + shots PASS.
  Note license: Segoe UI proprietary — thay bằng OFL (Inter) trước bản giao khách.
  File: `screens.c`, `fonts/ui_fonts.h`, `fonts/ui_font_sans*.c`, `tools/build_assets.bat`,
  `tools/ttf/SegoeUISemibold.ttf`, `sim_pc/CMakeLists.txt`, `tools/run_regression.ps1`,
  `docs/*.md`, `docs/screenshots/*`, `PLAN.md`.

- **UI · Sidebar phủ toàn bộ chiều cao màn hình** — Nav rail trái giờ chạy 0..480
  (`UI_SCREEN_H`, mỗi nút ~80px), top bar rút gọn chỉ phủ cột nội dung bên phải rail
  (pos 72,0, rộng 728). Khung nội dung 728x420 không đổi nên 6 tab không cần sửa.
  Regression đầy đủ PASS; 6 PNG + heap_summary đã refresh (heap không đổi).
  File: `screens.c`, `docs/SCREENS.md`, `docs/screenshots/*`, `PLAN.md`.

- **UI · Đồng bộ nền trang Graphs với các trang khác** — Bỏ palette riêng
  `GRAPH_COLOR_BG/CARD/BORDER` (page 0xF8F9FA, card 0xFCFCFC, border 0xC8CDD2): trang
  Graphs giờ dùng `COLOR_BG` / `COLOR_CARD_BG` / `COLOR_BORDER` chung như 5 tab còn lại.
  Giữ local các màu identity series (SPEED/Iq/Voltage/Irms) + grid + axis. Heap không
  đổi (61%); regression đầy đủ PASS; 6 tab PNG + heap_summary đã refresh.
  File: `screens.c`, `docs/screenshots/*.png`, `docs/screenshots/heap_summary.txt`,
  `PLAN.md`.

- **Docs · Re-align `docs/TELEMETRY.md` sau Phase 20** — Bổ sung mục "Lệnh Tx (`actions.c`)":
  START/STOP guard theo `motorCmd.bits.cmd` (command state) thay vì chỉ feedback,
  giải thích cửa sổ START→echo và check `offline_stop_bad` trong stress. Verify toàn bộ
  flow mới: build sim → regression (syntax 2 chế độ, position, Control headless modes,
  stress 120s, 6 shots) → `--mode`/`--ms` validation đều PASS. File: `docs/TELEMETRY.md`.

- **Audit · Post-AI repository review (Phase 20)** — Đối chiếu docs/tooling mới với
  source thật; sửa các mô tả sai về lane, telemetry, Settings và text storage. Vá race
  an toàn START→STOP khi chưa có status echo, loại fallback label có thể giữ stack
  pointer, thêm regression cho đường offline. Bổ sung `--mode` + validate CLI,
  harden `shoot_all`, và thay dependency logo đã mất bằng source chuẩn 16 KB có pixel
  giống hệt asset hiện hành. Strict syntax, 3 Control layout modes, stress 120s,
  6 shots, asset check và MCU manifest đều PASS. File: `actions.c`, `screens.c`, `ui.c`, `ui.h`, `sim_pc/main.c`,
  `tools/shoot_all.ps1`, `tools/run_regression.ps1`, `tools/build_assets.bat`,
  `tools/assets/ui_logo.png`, `tools/README.md`, `docs/*.md`, `images.h`, `CLAUDE.md`,
  `README.md`, `PLAN.md`.

- **Docs · Docs reorganization** — Đơn giản hóa `README.md` (chỉ giới thiệu + quick start
  + bảng trỏ tài liệu). Tách toàn bộ tài liệu chi tiết vào `docs/` theo chủ đề:
  `README.md` (index + hướng dẫn đọc, có nhắc `DEV_LOG.md`), `ARCHITECTURE.md`,
  `SCREENS.md`, `TELEMETRY.md`, `PERFORMANCE_MEMORY.md`, `BUILD_TEST.md`.
  Tạo `DEV_LOG.md` ở root. Cập nhật `AGENTS.md` (bắt buộc đọc docs index + ghi DEV_LOG).
  File: `README.md`, `docs/*.md`, `DEV_LOG.md`, `AGENTS.md`.

- **Tooling · `tools/shoot_all.bat`** — Một lệnh chụp cả 6 tab headless thành PNG +
  `heap_summary.txt` (mặc định ghi vào `docs\screenshots`). Options: `-Telemetry`
  demo/demo-run/none, `-OutDir`, `-SkipBuild`, `-Ms`. Số liệu heap trùng khớp với
  `docs/MCU_MEMORY_PROFILE.md`. File: `tools/shoot_all.ps1`, `tools/shoot_all.bat`,
  `tools/README.md`.

- **Simulator · `sim_pc.exe --layout <tab>`** — Chế độ dump cây widget dạng text
  (class, effective visibility — bao gồm HIDDEN của ancestor, tọa độ/kích thước tuyệt
  đối, text label, giá trị slider/arc/bar/dropdown) cho layout debug headless và CI.
  Refactor `boot_to_tab()` dùng chung với `--shot`. Regression toàn bộ vẫn PASS.
  File: `sim_pc/main.c`, `tools/README.md`.

---

### Mẫu entry

```markdown
## YYYY-MM-DD

- **Phạm vi · Tiêu đề ngắn** — Mô tả 1-3 dòng: vì sao, làm gì, kết quả. Nếu sửa code
  thì nêu đúng file + hành vi thay đổi, kèm cách verify (regression/shoot/layout).
  File: `path/file.c`, `docs/file.md`.
```
