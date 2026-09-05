# Kiến trúc & luồng điều khiển

File chính: `ui.c` / `ui.h` (điều phối), `screens.c` (nội dung + tick), `actions.c` (sự kiện).

## Tổng quan

- **Một LVGL screen duy nhất luôn resident** (`ui_MainScreen`) cùng một rebuild boundary
  trong suốt `ui_ContentHost` 712x414. Chuyển tab chỉ `lv_obj_clean(ui_ContentHost)` +
  build lại children; không giữ cả 6 tab resident (hidden widget vẫn tốn heap), cũng
  không invalidate lại toàn bộ shell 800x480.
- **Chrome bền vững trên `lv_layer_top()`**: header band trắng (58px tại (88,8)) +
  sidebar panel trắng nổi (80x464 tại (8,8), bo 14, shadow phẳng), tạo 1 lần ở
  `create_common_ui()`, không bao giờ bị phá. Sáu icon/pill navigation được vẽ bởi
  một custom widget duy nhất; widget tự map click theo sáu vùng Y và chỉ invalidate
  cell active cũ/mới. Vùng nội dung cố định:
  `UI_CONTENT_W = 712`, `UI_CONTENT_H = 414` (host tại x=88, y=66; card đầu tiên
  thường bắt đầu y=74 sau `UI_GAP`).
  Hệ spacing thống nhất: **mọi kẻ hở/margin = `UI_GAP` = 8px**; bề mặt card/panel
  không viền + shadow phẳng (10px, lệch 3px, đen 20%); viền 1px chỉ còn trên control
  chức năng (nút, dropdown, divider).
- **Teardown/rebuild KHÔNG bao giờ chạy trong event callback** — chỉ từ `ui_tick()`.

## Tab switching (coalesced + throttled)

```
click tab ──> action_tab_XXX() ──> ui_request_tab(tab)     (rẻ, chỉ ghi cờ)
                                        │
ui_tick() ──> ui_service_pending()      │
               • coalesce + throttle 90ms │
               ├─ phase 1: ui_tabbar_set_active(tab)
               │           └─ lv_refr_now(NULL): flush icon cũ/mới + title
               │
next ui_tick()                         └─ phase 2: ui_build_tab(tab)
                                                    ├─ ui_current_tab = tab
                                                    ├─ lv_obj_clean(ui_ContentHost)
                                                    ├─ check heap >= UI_HEAP_FLOOR (24 KB)
                                                    │   └─ nếu thiếu: ui_build_lowmem()
                                                    ├─ create_screen_XXX()
                                                    └─ ui_force_refresh()
```

Control có thêm đường same-tab có kiểm soát:
`ui_request_current_rebuild()` chỉ đặt `s_force_pending`; `ui_service_pending()`
thực hiện rebuild khi LEVEL↔POSITION đổi cấu trúc. SPEED↔TORQUE cùng dùng LEVEL tree
nên chỉ đổi nội dung/style tại chỗ. Event callback không bao giờ tự clean object đang
xử lý sự kiện.

Tap nhanh liên tiếp chỉ rơi vào **1 lần rebuild cho tab cuối**. Phase chrome
chạy từ `ui_tick()`, không phải event callback; sau khi force refresh nó return ngay để
periodic lane cũ không chen vào cùng batch. Tick sau mới clean/create content.
Baseline cũ clean `ui_MainScreen` gửi đúng 384,000 px / 768,000 B cho mọi switch.
Content host giữ tổng switch 318,192..329,280 px, nhưng nay tách thành **2
refresh**: chrome đầu 16,632..18,552 px và content burst 301,560..310,728 px. Tổng
byte không đổi; icon/title phản hồi trước burst content.

## Update lanes (`ui_tick()`)

`ui_tick()` gọi từ vòng lặp chính bao nhiêu lần cũng được; công việc chỉ chạy theo lane:

| Lane | Cadence | Việc gì | Ghi chú |
|---|---|---|---|
| SLOW | 200 ms (5 Hz) | Mọi text động + `tick_common_ui()` | Toàn bộ text số cập nhật tại đây |
| CHART | 125 ms (8 Hz) | Thu mẫu rolling history + cập nhật 1 chart mỗi lần | Round-robin 4 series → mỗi chart 2 Hz, so le |
| FAST | — | Không được schedule; enum giữ để tương thích tick API | Đừng khôi phục trừ khi có widget cần <200ms |

Quy tắc: **không đẩy SLOW lên nhanh hơn** — nó nhân lượng dirty-region lên xSPI
(bottleneck). `tick_screen_XXX()` chỉ được chạy khi `ui_current_tab` khớp; khi
low-memory fallback hoạt động (`s_lowmem_active`) thì mọi tick bị chặn vì static
pointer đã thành địa chỉ chết sau `lv_obj_clean(ui_ContentHost)`.

`ui_force_refresh()` chạy cả SLOW + CHART ngay sau khi build tab để màn hình mới đúng
ngay frame đầu (không chờ 200ms).

Direct manipulation là ngoại lệ có kiểm soát: Control/POSITION handle theo cadence
input (~33 Hz), nhưng track `lv_arc` là tĩnh và readout số chỉ 10 Hz + force ở
RELEASED. Không đẩy cadence này vào SLOW lane vì sẽ làm mọi màn hình redraw nhanh hơn.

## Telemetry & luồng lệnh

- **Tx**: UI sở hữu `motorCmd` (struct wire, `motor_comm.h`). Actions ghi thẳng bitfield;
  firmware chỉ việc gửi. Không có lớp command callback trung gian.
- **Rx**: `motorStatusFast` (100 ms) / `motorStatusSlow` (1000 ms) / `motorConnected` do
  UART parse điền. UI **chỉ đọc qua accessor**:
  `ui_motor_status_fast()`, `ui_motor_status_slow()`, `ui_motor_connected()` — Demo mode
  trả struct riêng, xem [TELEMETRY.md](TELEMETRY.md).
- **Không bao giờ gọi LVGL từ ISR.** UART chỉ commit frame vào buffer an toàn; mọi render
  từ `ui_tick()`.

## Các khối trạng thái UI-local

- `ui_runtime_*`: bộ đếm run time riêng UI (reset mỗi START được chấp nhận, freeze khi
  STOP/FAULT, xóa khi reboot). Đồng bộ cả với motorState echo.
- `ui_rs485_*`: cấu hình RS-485 đang chờ (baud/parity/stopbits) + hooks load/commit
  do firmware đăng ký trước `ui_init()`.
- `ui_tabbar_set_active()`: đổi active index, invalidate đúng nav cell cũ/mới và cập
  nhật page title. Không tạo/xóa sáu button con.

## Splash

`create_splash()` tạo overlay trên top layer, tự xóa sau ~2.5s bằng animation
(logo fade+slide → cả overlay fade out). Không ảnh hưởng màn hình phía sau.
