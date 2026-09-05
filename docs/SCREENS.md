# Từng màn hình + chrome dùng chung

File: `screens.c`. Mọi tab build trong `ui_ContentHost` trong suốt 712x414 tại
x=88, y=66; card đầu tiên thường bắt đầu y=74 sau `UI_GAP`. Nền page và shell sống
vĩnh viễn trên `ui_MainScreen`; mỗi `create_screen_XXX(parent)` chỉ gọi
`init_screen_content()` để reset buffer text, không chạm style full-screen. Sau đó
`tick_screen_XXX()` mỗi lane cập nhật giá trị — **tick không bao giờ tạo/xóa widget**.

## Chrome dùng chung (`create_common_ui`, sống trên `lv_layer_top()`)

- **Header band (trắng, frameless + shadow — cùng ngôn ngữ card, không border)**:
  logo Hyphen Deux + **tên trang hiện tại**
  (`DASHBOARD/MONITOR/CONTROL/GRAPHS/DIAGNOSTICS/SETTINGS` — sans22 ink, cập nhật bởi
  `ui_tabbar_set_active`) bên trái; bên phải **state chip** (trạng thái MOTOR trong
  chip nền tint contrast cao hơn + outline 1px theo trạng thái) + pill **"RS-485"**
  (neutral gray đậm hơn + outline 1px; caption cố định; **dot màu** là
  thứ thể hiện link state — bỏ chữ ONLINE/OFFLINE theo yêu cầu operator). Brand text
  "MOTOR CONTROL HMI" cũ đã bỏ — nó lặp lại trên mọi trang, không gian đó giờ dành
  cho tên trang. Baud rate không còn trên header — nó là chi tiết Settings.
- **Sidebar trái (panel trắng nổi 80x464 tại (8,8), bo 14, shadow)**: một widget
  custom-draw duy nhất vẽ 6 icon 44x44 Alpha-4 (dashboard/monitor/control/graphs/
  diag/settings — Settings là icon **bánh răng**) từ asset const trong Flash; mỗi icon
  nằm trong **pill 64x64 bo 16px**; tab active = **pill
  charcoal đặc (`COLOR_ACCENT`) + icon trắng** (khối đặc duy nhất trên rail), inactive
  = pill trong suốt + icon xám. Click được map vào 6 hit-band theo `TAB_BTN_Y`; chỉ
  cell active cũ/mới bị invalidate; draw callback cull năm cell không giao
  `clip_area`. Pill đầu/cuối cách mép panel đúng 8px, gap giữa 13/12px. Không
  còn 18 child button/pill/image resident. Khi đổi tab, sidebar + page title flush thành
  refresh nhỏ riêng trước khi content được teardown/build ở tick kế tiếp.
- **Nền + viền kiểu "thẻ nổi"**: page xám nhạt mát (`COLOR_BG` 0xE9ECF1), viền
  card/hairline nhẹ (`COLOR_BORDER` 0xC2C8D1), pill link dùng `COLOR_PANEL_BG`
  0xDFE5EC. Màu xanh lá dành cho START/healthy; SPEED dùng identity xanh dương.
- **Typographic hierarchy**: caption/button dùng `ui_font_sans20/22` (Segoe UI
  Semibold — proportional; thay bằng font OFL như Inter trước bản giao khách); mọi
  giá trị số/unit/giá trị kỹ thuật giữ `ui_font_mono*` (JetBrains Mono Bold).
- **Demo toggle (chỉ build demo)**: giữ brand ~800ms → bật/tắt demo_sim. Phải dừng motor
  và STOPPED ≥1s mới cho tắt demo (không handoff khi đang quay). Debounce 1.2s.

## Dashboard

- Hero card 696x300: ảnh mặt đồng hồ `ui_img_dashboard_gauge` (320x320, Alpha-4; arc
  ngoài radius 146, label scale 0..200 tại radius 114) + `lv_arc` sống (270°, range
  0..200 RPM, stroke 8). Arc dùng SPEED signal cyan `#00B7FF` giống thanh CONTROL
  dưới 85%, amber từ 85% đến hard limit,
  và chỉ đỏ khi telemetry vượt 200 RPM hoặc có wire fault; màu chỉ ghi lại khi đổi
  band. RPM mono66 đặt dưới tâm hình
  học của vòng ~16px để đọc như "giữa vòng" với arc hở đáy) + hướng (icon cw/ccw).
  Hai cánh của card: trái **MODE**, phải **TARGET** (giá trị + đơn vị theo mode —
  RPM / A / °), cả hai theo `motorCmd` (command-owned, dùng được khi mất link).
- 3 card: RUN TIME (H:M:S), IQ CURRENT (A), FAULTS (count, xanh khi 0 / đỏ khi >0) —
  giá trị cùng cỡ **mono38**, đẩy xuống dưới cho cân đối (pad_row 12).
- Tick (SLOW): RPM clamp 0..200, giá trị chỉ đổi khi khác (`label_set_if_changed`),
  direction đổi icon theo `dir` feedback; MODE/TARGET cập nhật khi command đổi.

## Monitor

- 4x2 grid, 8 card metric 168x195 (soft color zoning): IQ CURRENT, RMS CURRENT, SPEED, VOLTAGE, RUN TIME,
  DIRECTION (icon trung tính, offset +10), POWER, FAULTS. **Title căn giữa card**, vạch
  **header band 46px tint pastel** (theo metric, border-bottom accent 3px, title màu accent); value chỉ đổi màu khi có bất
  thường thật (điện áp < 12V đỏ, faults > 0 đỏ). Cỡ số: 6 card chính **mono44**, RUN
  TIME mono30 (chuỗi 8 ký tự không vừa 44); caption "HH:MM:SS".
- Tick: cập nhật tối đa **4 giá trị/lần SLOW** (so le 2 phase) để giới hạn dirty-region.

## Control

- Một card trắng lớn: **hàng MODE trên cùng** (nhãn "MODE" trái + segmented control
  SPEED/TORQUE/POSITION phải, căn đúng cột phải, pad_hor 2 để chữ không sát mép nút)
  → vùng readout+control → action bar START/STOP dưới cùng (ngoài card, fixed).
- Chỉ subtree của mode đang dùng được tạo: SPEED/TORQUE dùng chung **LEVEL tree**,
  POSITION dùng **POSITION tree**. LEVEL↔POSITION đặt yêu cầu rebuild same-tab và
  chỉ clean/build từ `ui_tick()`; SPEED↔TORQUE cập nhật cùng tree tại chỗ. Không còn
  subtree inactive bị `HIDDEN` nhưng vẫn chiếm heap.
- **LEVEL layout** (SPEED/TORQUE): **2 instrument card trắng xếp dọc** (SPEED trên,
  IQ CURRENT dưới, gap 12). Mỗi title label được tái dùng làm header navy 46px +
  underline SIGNAL cyan/teal, giống Monitor/Graphs nhưng **không thêm header object**;
  body xuyên nền trắng card cha nhưng có outline 1px bo góc bao trọn từng instrument,
  giúp hai vùng không lẫn vào nền mà vẫn không thêm wrapper/shadow/fill; giá trị mono56
  ink + unit phải, progress bar sáng sát đáy. Bar đổi màu theo band: <85% signal,
  85-100% amber, RED chỉ khi có fault hoặc
  vượt hard limit — chạm configured limit không bị đỏ. Cột điều khiển phải được
  bọc trong surface xám `#EDF2F7` như layout đã review (rộng **280px**; MODE
  segmented rộng riêng **360px**):
  TARGET slider + LIMIT slider ("Iq Limit" / **"Speed Limit"**) + FWD/REV.
  Ảnh review: [control.png](screenshots/control.png) và
  [control_torque.png](screenshots/control_torque.png).
- **POSITION layout**: vùng ring vẫn **transparent**, dùng trực tiếp nền trắng của
  Control card; cột Iq Limit dùng cùng surface xám `#EDF2F7` với LEVEL để phân nhóm
  control. Không còn deck xanh/viền SIGNAL. Ring 212px là **track tĩnh** `COLOR_BORDER_MUTED`
  (#B6C3D2, indicator transparent) + handle navy/charcoal 24px (#17324F), số dùng
  ink đen; caption "TARGET ANGLE" đã bỏ theo operator. `ctrl_pos_math.h` tính
  góc liên tục 360°; target giữ ở `ctrl_pos_raw`, không gọi `lv_arc_set_value()` khi
  kéo nên track tĩnh không bị invalidate vô ích. Handle bám đủ cadence input (~33 Hz),
  số góc mono44 giới hạn 10 Hz và force giá trị cuối ở RELEASED; cột Iq Limit căn giữa.
  Ảnh review riêng: [control_position.png](screenshots/control_position.png).
- **FWD/REV là hai nút icon quay CW/CCW, không caption** — icon 34px căn giữa nút;
  state active dùng charcoal + icon trắng, state còn lại nền trắng + icon ink.
- **Disabled states thật** (operator review): button mà logic bỏ qua PHẢI trông không
  bấm được — motor dừng thì STOP mờ (`LV_STATE_DISABLED`), motor chạy thì START mờ,
  FAULT khóa START (cả `action_motor_start` lẫn giao diện). Trong STARTING/STOPPING
  khóa luôn MODE, direction và các slider để không chồng lệnh lên chuyển tiếp.
  STOP luôn sẵn sàng khi có lệnh đang treo. Cache trạng thái (`ctrl_last_start_ok`
  / `ctrl_last_stop_ok` / `ctrl_last_locked`) là screen state và reset 0xFF mỗi lần
  rebuild Control.
- `ctrl_mode_settings[]` là cache sống vĩnh viễn (process lifetime): mỗi mode giữ
  target/limit riêng theo đúng đơn vị vật lý của mode đó; đổi mode qua lại không mất
  giá trị và không diễn giải raw value của mode cũ bằng đơn vị của mode mới.
- Mode là lệnh: layout theo `motorCmd.opMode` ngay (không chờ echo) để dùng được khi
  không kết nối. Slider khi operator đang giữ thì tick không ghi đè.

## Graphs

- 2x2 chart card bo 12: header instrument navy #132238 cao 46px, full-width sát
  mép trên/trái/phải và được card clip đúng hai góc trên như Monitor; title + live
  value + underline 3px dùng SIGNAL color của metric. Plot giữ nền #F8FAFC, grid
  #D8DEE7, trace 3px, cột min/max trái và chart line 50 điểm.
- Scale: SPEED 0..200 RPM; Iq & Irms 0..10 A (rộng hơn trần 3A để thấy transient);
  VOLTAGE 0..80 V. Title chuẩn hóa: `SPEED`, `IQ CURRENT`, `VOLTAGE`, `RMS CURRENT`.
- Rolling history `g_hist[4][50]` thu **mọi CHART tick dù tab nào đang mở** → vào tab là
  đã có đồ thị đầy. Chỉ khi Graphs active mới chạm chart widget (pointer thường là
  dangling sau `lv_obj_clean`).

## Diagnostics

- Banner tóm tắt (xanh "ALL SYSTEMS NORMAL" / đỏ "FAULT DETECTED", đổi theo trạng thái)
  + card FAULT REGISTER: **grid 2 cột** (trước 3 cột để lại khoảng trống lớn ở hàng
  cuối), 5 dòng fault (OVERCURRENT, OVERVOLTAGE, UNDERVOLTAGE, OVER TEMP, RS-485 LINK).
  Mỗi dòng: icon + tên + check/x.
- **Blink chỉ cho fault mới**: bit fault chuyển 0→1 thì mark blink ~6 s rồi đứng yên
  màu đỏ — fault đã nhận biết không blink vô hạn (đỡ mỏi mắt). Fault hết thì về check
  xanh ngay.
- CLEAR FAULTS: **chưa có wire command** → nút DISABLED "CLEAR UNAVAILABLE" **nằm dưới
  card lỗi**. Surface + shared shadow nằm trên wrapper normal-state; button trong suốt
  bên trong vẫn giữ `LV_STATE_DISABLED`, nên opacity của state disabled không làm mất
  shadow. Không để nút active mà bấm không có tác dụng. TODO trong
  `actions.c` khi protocol bổ sung message.

## Settings

- Card RS-485 toàn trang: 3 dropdown (BAUD RATE 8 giá trị, PARITY None/Even/Odd, STOP
  BITS 1/2), một hàng LINK STATUS read-only 3 trạng thái, và SAVE / LOAD DEFAULTS.
  Hàng được dàn **đều theo chiều cao card** (`LV_FLEX_ALIGN_SPACE_EVENLY`). Hàng nút SAVE/LOAD DEFAULTS nằm **dưới card** (ra ngoài frame, có shadow).
- Dropdown ghi vào các giá trị pending `ui_rs485_baud/parity/stopbits` (giá trị thật,
  không phải index).
- **SAVE**: disabled thật khi chưa có thay đổi (dirty snapshot `s_rs485_saved`, sync
  lại khi build/dropdown/reset/save); thành công → "SAVED" xanh + icon check ~1.5 s,
  hook lỗi → "FAILED" đỏ. **Safety interlock**: motor chưa STOPPED (hoặc lệnh đang
  treo) thì SAVE báo **"STOP FIRST"** (warn) — không đổi UART khi motor đang chạy.
  (Không còn label "UNSAVED CHANGES" — operator bỏ.)
- **LOAD DEFAULTS**: chỉ sửa pending về mặc định 921600/None/1,
  phải SAVE mới áp dụng/persist qua hook `ui_rs485_commit_cb` trên MCU.

## Splash

Xem [ARCHITECTURE.md](ARCHITECTURE.md) — overlay tự xóa.
