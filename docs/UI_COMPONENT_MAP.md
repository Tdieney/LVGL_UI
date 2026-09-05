# Bản đồ component UI — bảng quản lý

Mục đích: liệt kê **từng thành phần nhìn thấy** trên màn hình để tiện quản lý/thay
đổi. Cột quan trọng nhất là **"Ai bố trí?"**:

- **Code** = widget LVGL sống (`lv_*`), vị trí/kích thước/trạng thái đặt trong
  `screens.c` (có thể sửa trực tiếp trong code, đổi vị trí/ màu/ font).
- **Asset tĩnh** = file `ui_icon_*.c` / `ui_img_*.c` / `fonts/*.c` sinh từ
  `tools/build_assets.bat` — **không sửa file `.c` bằng tay**; muốn đổi hình thì
  sửa generator (`tools/gen_icons.py`, `tools/gen_dashboard_gauge.py`) rồi chạy
  lại asset pipeline, sau đó build.

## Chrome chung (`create_common_ui` — sống trên `lv_layer_top()`, tạo 1 lần)

| Component | Loại | Ai bố trí? | Ghi chú |
|---|---|---|---|
| Header band (khung trắng 712x58) | `lv_obj` + shadow | Code | Vị trí (88, 8), bo góc 0 (không bo) |
| Logo Hyphen Deux (2 vạch) | 2× `lv_obj` màu | Code | Màu ink + brand red |
| Tên trang ("DASHBOARD"...) | `lv_label` | Code | Cập nhật bởi `ui_tabbar_set_active` |
| State chip MOTOR (STOPPED/RUNNING...) | `lv_obj` + `lv_label` | Code | Tint contrast cao hơn + outline 1px theo state, tick SLOW |
| Pill RS-485 (dot + chữ) | `lv_obj` + `lv_obj` dot + `lv_label` | Code | Neutral gray + outline 1px; chữ cố định, dot đổi màu theo link |
| Navigation rail (khung trắng 80x464, bo 14) | `lv_obj` + shadow | Code | Vị trí (8,8), panel nổi |
| Navigation visual + hit map | Một `lv_obj` custom-draw/click | Code | Vẽ 6 pill/icon theo `TAB_BTN_Y`; chỉ invalidate cell cũ/mới; không có 18 child widget |
| 6 icon tab (dash/mon/ctrl/graph/diag/set) | Asset alpha-4 44x44 | Asset (`gen_icons.py`) | Map const trong Flash; custom widget recolor trắng/xám theo state |
| Splash (logo lúc boot) | `lv_obj` + asset logo | Asset (`ui_logo.png`) | Overlay tự xóa ~2.5s |
| Content host 712x414 | `lv_obj` trong suốt, persistent | Code (`ui.c`) | Vị trí (88,66); chỉ children bị clean/rebuild khi đổi tab |

## Dashboard

| Component | Loại | Ai bố trí? | Ghi chú |
|---|---|---|---|
| Hero card 696x300 | `lv_obj` + shadow | Code | Bo 14, nền trắng |
| Mặt số gauge (arc xám, vạch, số 0..200, motif BLDC) | Asset alpha-4 320x320 | Asset (`gen_dashboard_gauge.py`) | Recolor ink; vị trí code (0, +12) |
| Arc SPEED sống (0..200 RPM) | `lv_arc` | Code | Stroke 8; SIGNAL cyan #00B7FF như Control <85%, amber ≥85%, red khi fault hoặc vượt hard limit |
| Số speed (mono66) | `lv_label` (buffer pool) | Code | Vị trí (0, +16) |
| Chữ "RPM" (mono22) | `lv_label` tĩnh | Code | Vị trí (0, +66), màu ink |
| Icon direction (cw/ccw) | Asset alpha-4 34x34 | Asset (`gen_icons.py`) | Vị trí (0, +112), đổi src theo `dir` |
| Stack MODE (caption + giá trị) | `lv_label` ×2 | Code | Theo `motorCmd.opMode` |
| Stack TARGET (caption + giá trị + đơn vị) | `lv_label` ×3 | Code | Theo `motorCmd.ctrlValRaw` |
| 3 card dưới (RUN TIME / IQ CURRENT / FAULTS) | `lv_obj` + shadow + labels | Code | Kích thước 226x90, giá trị mono38 |

## Monitor

| Component | Loại | Ai bố trí? | Ghi chú |
|---|---|---|---|
| 8 card metric 168x195 | `lv_obj` + shadow | Code | Bo 12 |
| Vạch màu identity (142x6) | `lv_obj` | Code | Màu theo metric; vị trí (0, +12) |
| Title card | `lv_label` tĩnh | Code | Neutral; vị trí (0, +26) |
| Giá trị (mono44; RUN TIME mono30) | `lv_label` (buffer pool) | Code | Tick SLOW, tối đa 4 giá trị/lần |
| Đơn vị | `lv_label` tĩnh | Code | Đáy card |
| Icon direction (card DIRECTION) | Asset alpha-4 | Asset (`gen_icons.py`) | Recolor `COLOR_TEXT_M` |

## Control

Chỉ layout active tồn tại trong heap: SPEED/TORQUE dùng chung LEVEL subtree;
POSITION dùng subtree riêng. LEVEL↔POSITION rebuild deferred qua `ui_tick()`, còn
SPEED↔TORQUE cấu hình lại cùng tree tại chỗ.

| Component | Loại | Ai bố trí? | Ghi chú |
|---|---|---|---|
| Card trắng lớn | `lv_obj` + shadow | Code | Flex column |
| Hàng MODE (nhãn + 3 nút SPEED/TORQUE/POSITION) | `lv_label` + `lv_btn` ×3 | Code | Active = charcoal + chữ trắng |
| Card SPEED (header + số mono56/unit + bar đáy) | `lv_label` ×2 + `lv_bar` | Code | Body transparent/trắng + shared outline 1px bo góc; title label kiêm header navy 46px + underline cyan, không thêm object |
| Card IQ CURRENT (header + số mono56/unit + bar đáy) | `lv_label` ×2 + `lv_bar` | Code | Body transparent/trắng + shared outline 1px bo góc; title label kiêm header navy 46px + underline teal; 0..3 A |
| Surface command bên phải | `lv_obj` | Code | `#EDF2F7`, bo 12; dùng cho cả LEVEL và POSITION |
| Nhóm TARGET (title, số, slider) | `lv_label` ×2 + `lv_slider` | Code | Container con transparent trên surface command; đơn vị đổi theo mode |
| Nhóm LIMIT (title, số, slider) | `lv_label` ×2 + `lv_slider` | Code | Container con transparent; "Iq Limit" (SPEED) / "Speed Limit" (TORQUE) |
| Nút FWD/REV (icon-only CW/CCW) | `lv_btn` + asset | Code + Asset | Icon alpha-4 34px căn giữa |
| Ring POSITION 212px | `lv_arc` track tĩnh (display-only) | Code | Nền card trắng xuyên qua container transparent; track #B6C3D2, handle #17324F; target không ghi vào arc để tránh invalidate track |
| Chấm đen trên ring | `lv_obj` | Code | Bám cadence input; vị trí tính từ `ctrl_pos_raw` |
| Readout góc (số + "°") | `lv_label` ×2 | Code | 0.1°/LSB |
| Slider Iq Limit (POSITION) | `lv_slider` | Code | Ghi `limitRaw` |
| Nút START / STOP (bằng nhau) | `lv_btn` + shadow + `lv_label` | Code | Disabled state thật theo trạng thái motor |

## Graphs

| Component | Loại | Ai bố trí? | Ghi chú |
|---|---|---|---|
| 4 card chart (SPEED / IQ CURRENT / VOLTAGE / RMS CURRENT) | `lv_obj` + shadow | Code | Bo 12 + clip corner; header navy full-width 46px như Monitor |
| Title + live value | `lv_label` ×2 | Code | SIGNAL color theo metric trên header navy |
| Cột min/max (trục Y) | `lv_label` ×2 | Code | Static text |
| Đường chart 50 điểm | `lv_chart` | Code | Round-robin 1 series/CHART tick |
| Đường kẻ lưới | style `lv_chart` | Code | Màu `GRAPH_COLOR_GRID` |

## Diagnostics

| Component | Loại | Ai bố trí? | Ghi chú |
|---|---|---|---|
| Banner tóm tắt (icon + text + "N / 5 OK") | `lv_obj` + asset + `lv_label` ×2 | Code + Asset | Tint xanh/đỏ theo fault |
| Card FAULT REGISTER | `lv_obj` + shadow | Code | Bo 10 |
| 5 dòng fault (grid 2 cột) | `lv_obj` cell + asset icon + `lv_label` ×2 | Code + Asset | Icon bolt/thermo/link |
| Mark check/x (blink fault mới) | Asset alpha-4 | Asset (`gen_icons.py`) | Animation `lv_anim` 6s |
| Khung CLEAR UNAVAILABLE | wrapper `lv_obj` + shadow, `lv_btn` disabled + `lv_label` | Code | Wrapper giữ shadow khỏi opacity disabled; chờ wire command |

## Settings

| Component | Loại | Ai bố trí? | Ghi chú |
|---|---|---|---|
| Card RS-485 696x398 | `lv_obj` + shadow | Code | Bo 10, `SPACE_EVENLY` |
| Section label "RS-485 CONFIGURATION" | `lv_label` + hairline | Code | Hairline là `lv_obj` 1px |
| 3 dropdown (BAUD/PARITY/STOP BITS) | `lv_dropdown` | Code | Chevron = asset alpha-4 |
| Hàng LINK STATUS (dot + text) | `lv_obj` + `lv_label` | Code | Theo `ui_motor_connected` |
| Nút SAVE (icon check + chữ) | `lv_btn` + shadow + asset + `lv_label` | Code + Asset | Disabled khi sạch; SAVED/FAILED/"STOP FIRST" tạm 1.5s; khóa khi motor chạy |
| Nút LOAD DEFAULTS | `lv_btn` + shadow + `lv_label` | Code | Chỉ sửa pending |

## Nguyên tắc quản lý

1. **Asset (`ui_icon_*`, `ui_img_*`, `fonts/*`)** → sửa generator trong `tools/`,
   chạy `tools/build_assets.bat` (hoặc từng bước như phần trên), review kích
   thước file trước khi commit. Không sửa file `.c` asset bằng tay.
2. **Widget (mọi thứ còn lại)** → sửa trong `screens.c`; vị trí/kích thước là số
   tuyệt đối hoặc flex, đều nằm trong `create_screen_*` / `create_common_ui` /
   `ui_tabbar_set_active`.
3. **Trạng thái động** (số liệu, màu theo state, disabled) → các hàm
   `tick_screen_*` chạy theo lane SLOW/CHART trong `ui_tick()` — tick **không**
   tạo/xóa widget.
4. Hệ spacing: `UI_GAP` = 8px cho mọi kẻ hở, viền 1px cho control chức năng,
   shadow phẳng (rộng 10, lệch 3, đen 20%) cho mọi bề mặt card/panel.
