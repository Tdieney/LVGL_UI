# Hiệu năng & kỷ luật bộ nhớ

Đây là các **ràng buộc thiết kế** (guardrail) — vi phạm sẽ bị regression hoặc review chặn.

## Nền tảng

- MCU 80 MHz (không FPU), 128 KB SRAM, 1 MB Flash, xSPI display có GRAM riêng.
- **Bottleneck là xSPI dirty-region** (số pixel phải flush mỗi frame), không phải CPU.
- Heap LVGL **42 KB** (`UI_LVGL_HEAP_BYTES`), 1 draw buffer **800x10 RGB565 = 16 KB**
  (`UI_DRAW_BUF_PIXELS`). Xem [MCU_MEMORY_PROFILE.md](MCU_MEMORY_PROFILE.md) để tích hợp.

## Kỷ luật heap (bắt buộc)

1. **Không cấp phát trong tick loop.** Text số dùng buffer pool cố định 12×24 byte
   (`label_bind_buffer()` → `label_set_if_changed()`). Cấm quay lại `lv_label_set_text()`
   cho giá trị động mỗi tick (nó alloc/free string đúng-size mỗi lần đổi → churn + phân mảnh).
2. **Caption tĩnh = `lv_label_set_text_static()`** → chuỗi phải trỏ literal hoặc
   storage tồn tại suốt process. Không để pointer treo.
3. **Assets `const`** (image maps/descriptors, font bitmaps, LUT, bảng con trỏ literal)
   để linker đưa vào Flash. Font mới phải thêm vào `sim_pc/CMakeLists.txt`; export
   MCU glob `fonts\ui_font_*.c` tự nhặt.
4. **Hidden widget vẫn tốn heap** → không giữ 6 tab resident. Control chỉ tạo subtree
   đang dùng: SPEED/TORQUE dùng chung LEVEL; POSITION có subtree riêng. LEVEL↔POSITION
   yêu cầu rebuild deferred từ `ui_tick()`, SPEED↔TORQUE cập nhật cùng tree tại chỗ.
5. **Shared styles** (`st_btn_geom`, `st_listrow`, `st_sld_*`) thay cho hàng loạt
   `lv_obj_set_style_*` tĩnh trên từng object — mỗi object giữ 1 style-list entry thay
   vì mảng local property; giảm alloc/frag khi rebuild tab. Chỉ STATIC property nằm đây;
   thứ tick đổi màu thì để local.
6. **Không framebuffer 800x480 trên MCU** — display có GRAM; draw buffer từng phần.

## Kỷ luật CPU / phép toán

- Integer/fixed-point trong mọi hot path: `fmt_scaled()` (không float printf),
  `sin_lut`, `ctrl_pos_math.h` (integer atan2). Cấm `sinf/cosf/atan2f`/float format
  trong tick/input hot path.
- Đổi dữ liệu thì đổi ít nhất: `label_set_if_changed`, `label_color_if_changed`,
  band-guard cho bar fill, `link_state_normalize` cache.

## Cadence update (xSPI load)

- SLOW 5 Hz: toàn bộ text. Monitor tự giới hạn **4 giá trị/settled tick** (so le 2 phase).
- CHART 8 Hz: 1 chart/card mỗi tick (round-robin 4 series, mỗi series 2 Hz so le) — không
  bao giờ vẽ 2 chart cùng frame.
- FAST không được schedule (enum còn giữ để tương thích tick API); nếu cần chuyển động
  <200ms thì thêm lane mới, đừng tăng tốc SLOW.

## Số đo dirty-region / xSPI

Simulator có `--profile <tab>` đếm pixel thực đi qua `flush_cb` sau 4 giây settle
(draw buffer đúng 800×10 như MCU). Demo RUN, cửa sổ đo 10 giây:

| Trường hợp | Pixel/s | Burst lớn nhất | Ghi chú |
|---|---:|---:|---|
| Dashboard | 19,956 | 36,786 px | RPM đã gần settle; chỉ vùng thực sự đổi |
| Monitor | 74,466 | 24,920 px | Tối đa 4 value mỗi SLOW tick |
| Control SPEED | 5,913 | 14,784 px | White body + static 1px frame; telemetry đã settle |
| Control TORQUE | 5,913 | 14,784 px | Cùng layout/runtime budget với SPEED |
| Graphs | 337,684 | 45,671 px | 4 chart × 2 Hz, SHIFT redraw toàn plot |
| Diagnostics / Settings | 0 | 0 px | Không đổi state trong cửa sổ đo |
| Control POSITION drag | **93,433** | **10,006 px** | Handle ~33 Hz, số 10 Hz |

Tab-switch profile (một switch đã settle, không quy đổi px/s): kiến trúc cũ
`lv_obj_clean(ui_MainScreen)` luôn gửi **384,000 px / 768,000 B / 48 calls**. Host
712x414 persistent giữ tổng switch ở **318,192..329,280 px /
636,384..658,560 B / 42..43 calls**, giảm 14.25..17.14%. Tab switch nay có đúng
**2 refresh batch**: icon cũ/mới + title trước (**16,632..18,552 px**), content sau
(burst **301,560..310,728 px**). Tổng traffic không đổi nhưng chrome phản hồi
trước burst xSPI lớn. Regression khóa total ≤340k, chrome ≤22k, content burst
≤315k và bắt buộc `frames=2`.

POSITION trước tối ưu gửi 274,280 pixel/s và burst 51,076 px: dù indicator của
`lv_arc` transparent, việc ghi value vẫn invalidate arc 212px; label mono44 cũng bị
redraw theo mọi mẫu touch. Target nay giữ riêng ở `ctrl_pos_raw`, track không đổi,
handle vẫn bám input và số force đúng khi release. Regression khóa ≤120k pixel/s /
≤15k burst. Graphs là tải steady-state lớn nhất do semantics rolling SHIFT; muốn giảm
thêm phải chốt trade-off cadence/circular trace, raster nền tĩnh không làm nhỏ bbox
chart bị invalidate.

Hai feedback tile SPEED/IQ trước đây có background dark full-size nhưng LVGL chỉ
redraw dirty bbox của label/bar, không resend cả tile mỗi tick: đổi sang white body +
header navy làm steady traffic chỉ đổi 5,936 → 5,928 px/s. Outline 1px bọc mỗi tile
hiện tại không thêm object/body fill và số đo còn 5,913 px/s. Lợi ích chính là bỏ opaque
body fill trong draw stack và đồng bộ visual. Khôi phục surface xám riêng cho command
column không đổi traffic đo được (vẫn 5,913 px/s); regression vẫn khóa SPEED/TORQUE
≤10k px/s, burst ≤16k để bắt full-tile invalidation nếu tái xuất hiện.

### Khi nào nên raster hóa

- Giữ asset cho hình học phức tạp ít thay đổi (gauge face, icon, logo) như hiện tại.
- Màu trắng và xám opaque đều là 16 bit/pixel, nên đổi màu không giảm byte xSPI.
  POSITION giữ ring container transparent; command column dùng lại surface xám để
  phân nhóm. Lợi ích của ring transparent là CPU/style nhỏ, còn giảm traffic chuyển
  tab đến từ content-host invalidation boundary.
- Không bake blanket navigation/header/shadow: chrome đã resident và gần như không
  redraw sau settle, nên ảnh không giảm xSPI runtime. Navigation hiện là **một widget
  custom-draw**: pill active được vẽ bằng `lv_draw_rect`, sáu icon vẫn dùng các map
  Alpha-4 const có sẵn trong Flash, và click được map theo sáu vùng Y. Cách này giữ
  đúng hình ảnh nhưng bỏ 18 child button/pill/image và tiết kiệm khoảng 6.4 KB heap
  resident. Bake cả rail thành mask Alpha-4 80×464 vẫn tốn ~18.6 KB Flash, phải blend
  ảnh lớn và không giảm dirty pixels của lần đổi tab.
- POC sinh damage-map pairwise từ screenshot rồi vô hiệu invalidation lúc rebuild đã
  bị loại: LVGL merge các rectangle và vùng động làm một số switch tăng lên 345–379k
  px, đồng thời có nguy cơ để lại pixel cũ. Production giữ invalidation chuẩn với
  318,192..329,280 px/switch và guard ≤340k.
- Không bake cả 800×480: một ảnh RGB565 đã **768 KB**; sáu tab là 4.6 MB, vượt Flash
  1 MB. Chỉ riêng vùng content 712×414 cũng gần 590 KB/tab.
- Alpha-4 giảm còn 0.5 byte/pixel nhưng chỉ lưu mask, vẫn phải blend/recolor và sáu
  content mask đã khoảng 884 KB trước font/code/assets.
- Ảnh lớn không tự giảm byte xSPI: LVGL vẫn clip/redraw theo dirty bbox của widget
  động phủ trên ảnh. Solid fill/card code thường rẻ CPU hơn Alpha blend. Chỉ bake
  một cụm khi nó thay được nhiều primitive phức tạp và số Flash đo được đáng giá.

## Số đo đã kiểm chứng (simulator, heap 42 KB)

| Screen | Used | Free | Biggest | Frag |
|---|---:|---:|---:|---:|
| Dashboard | 52% | 20,800 B | 20,640 B | 1% |
| Monitor | 62% | 16,696 B | 15,904 B | 5% |
| Control SPEED/TORQUE | 73% | 11,776 B | 9,384 B | 21% |
| Control POSITION | 58% | 18,464 B | 16,200 B | 13% |
| Graphs | 66% | 14,880 B | 14,088 B | 6% |
| Diagnostics | 54% | 20,208 B | 19,184 B | 6% |
| Settings | 57% | 18,832 B | 16,816 B | 11% |

Guardrail stress (`--stress-demo`, 120 s, cycle 3 Control layouts): **min free ≥ 8 KB,
min biggest block ≥ 8 KB** (thực đo hiện tại: 11,368 B / 10,200 B, frag-max 14%).
Control LEVEL 73% used / 21% frag là điểm cần theo dõi — shadow phẳng 10px có chi phí
vẽ, cần đo dirty-pixel thật trên xSPI trước khi thêm widget vào màn hình này.

`ui_build_tab()` có circuit breaker: sau `lv_obj_clean(ui_ContentHost)`, nếu heap < 24 KB → hiện màn
hình "LOW MEMORY" thay vì build nửa chừng (đẹp hơn crash).

## Tránh các cạm bẫy đã từng gặp

- Click tab nhanh liên tiếp → thrash heap: đã fix bằng coalesce (xem ARCHITECTURE).
- Demo/UART ghi chung struct → torn fields: đã tách 2 nguồn + accessor.
- `lv_arc` full-circle drag → snap ở seam: thay bằng math riêng (`ctrl_pos_math.h`).
- `lv_chart_set_axis_tick` → tick label vẽ ngoài layout box bị clip: thay bằng label
  min/max thường.
- Dropdown arrow mặc định (LV_SYMBOL_DOWN) chỉ có trong font Montserrat → dùng ảnh
  chevron Alpha-4 riêng, giữ widget ở font mono.
