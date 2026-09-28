> **Lưu trữ — chỉ mang tính lịch sử.** Bản tóm tắt tiếng Việt của lần giao ngày 2026-09-23; báo cáo gốc là [REVIEW.md](REVIEW.md). Trang Trends đã thay đổi sau đó (D-040..D-043 trong [UI_DESIGN_BRIEF.md](../../UI_DESIGN_BRIEF.md)).

# Walkthrough: Tối Ưu Độ Mượt Trends & Khắc Phục Lỗi Thời Gian Lịch Sử

Đã hoàn thành toàn diện việc đo lường, tối ưu hóa độ phản hồi của trang Trends, xử lý 3 lỗi thời gian lịch sử, khắc phục lỗi dọn tiến trình verifier trên Windows, và bổ sung bộ kiểm thử tự động (Checks 36..39) trong đúng **MỘT đợt giao hàng hợp nhất**, tuân thủ nghiêm ngặt mọi nguyên tắc kiến trúc:
- **0 git commit, 0 git push, không sửa nhánh motor-control**.
- **Giữ nguyên 100% giao diện Quiet Architecture đã được nghiệm thu**: không đổi bảng màu, không co kéo layout, toàn bộ text $\ge 18\text{px}$.
- **Hợp đồng vô tuyến (Wire contract) 8B / 20B giữ nguyên**.

---

## 1. Kết Quả Các Cổng Kiểm Thử Tự Động (100% Pass)

| Cổng kiểm tra | Lệnh thực thi | Kết quả | Ghi chú |
|---|---|---|---|
| **Asset Check** | `python tools/generate_assets.py --check` | **PASS** | 25 canonical masters, 68 runtime icons hợp lệ. Exit code 0. |
| **Simulator Build** | `tools/build_sim.bat` | **PASS** | Biên dịch GCC `-Wall -Wextra -Werror` sạch sẽ, 0 warnings. Exit code 0. |
| **Regression Suite** | `tools/run_regression.bat -SkipBuild` | **PASS** | **39/39 checks passed (100%)**. 0 rò rỉ bộ nhớ qua 50 chu kỳ, 27 ảnh scenario framebuffer tạo thành công. Exit code 0. |
| **MCU Export** | `tools/export_mcu.bat --verify` | **PASS** | Manifest MCU export khớp 100%. Exit code 0. |
| **Production Verifier** | `tools/verify_production.bat` | **PASS** | Vượt qua 4/4 pha (collision safety, isolated Release build `-Werror`, smoke test exit 0, negative control exit 1). Exit code 0. |

---

## 2. Chi Tiết Khắc Phục Kỹ Thuật

### P1 — Tối Ưu Hóa Độ Phản Hồi & Giảm Vẽ Thừa Trang Trends (`ui.c`)
- **Nguyên nhân gốc gây tốn tài nguyên khi đổi metric:**
  - `update_trends_page_widgets()` gọi lặp `update_trends_tabs()`.
  - `lv_chart_set_point_count()` và `lv_chart_refresh()` bị gọi vô điều kiện, kích hoạt chu kỳ invalidate toàn bộ đối tượng biểu đồ ngay cả khi dữ liệu hoặc thang đo không đổi.
  - Các thẻ chọn metric (CO₂, VOC, Temp, Humid) dùng `lv_obj_remove_style` và `lv_obj_add_style` động, gây phân bổ/giải phóng bộ nhớ heap không cần thiết khi chạm chuyển thẻ.
- **Giải pháp tối ưu:**
  - Bỏ lời gọi trùng lặp `update_trends_tabs()` trong `update_trends_page_widgets()`.
  - Kiểm tra `count != lv_chart_get_point_count(s_trends_chart)` trước khi gọi `lv_chart_set_point_count()`.
  - Bỏ qua `lv_chart_refresh()` nếu `range_changed` đã kích hoạt refresh biểu đồ trước đó.
  - Chuyển cơ chế style của thẻ chọn metric sang kiểu khai báo trạng thái tĩnh: gán `ui_style_card_porcelain` cho `LV_STATE_DEFAULT` và `ui_style_card_hero` cho `LV_STATE_CHECKED`. Khi chuyển thẻ, chỉ cần bật/tắt cờ `LV_STATE_CHECKED`, loại bỏ 100% chi phí style động.
  - Tái chọn thẻ đang mở giữ nguyên tính chất no-op tuyệt đối: **0 pixels, 0 bytes, 0 flushes**.
  - Kiểm thử chuyển thẻ 100 lần liên tục: **Độ trôi bộ nhớ heap = 0 byte** (Heap free trước: 13,768 B; sau: 13,768 B).

### P2 — Khắc Phục 3 Lỗi Thời Gian Lịch Sử (`ui.c`)
1. **Lỗi 1: Mất dữ liệu dài (>16 phút) làm lệch trục thời gian:**
   - *Hiện tượng:* Khi mất dữ liệu từ phút 0 đến phút 100, buffer cũ chèn các slot rỗng bắt đầu từ phút 1..15 rồi nhảy đến phút 100, khiến biểu đồ hiển thị sai bước thời gian 1 phút.
   - *Khắc phục:* Đổi công thức tính slot rỗng sang `gap_start = session_min - 15`. Mọi slot rỗng chèn vào luôn đại diện cho đúng cửa sổ 16 phút gần nhất `[session_min - 15 .. session_min - 1]` với điểm dữ liệu mới nằm ở slot 15. Thuật toán $O(16)$ giới hạn cứng.
2. **Lỗi 2: Thời gian phiên > 24 giờ quay về `+0m`:**
   - *Hiện tượng:* Lưu `session_min % 1440u` khiến mốc 24h (1440 phút) bị cuộn về 0.
   - *Khắc phục:* Lưu phút đơn điệu `(uint16_t)session_min` trong `minute_of_day` (hỗ trợ tới 65,535 phút = 45.5 ngày). Khi $\ge 60$ phút, nhãn hiển thị định dạng `+%uh%02um` (ví dụ `+24h00m`, `+100h00m`). Giữ nguyên `% 1440u` nghiêm ngặt cho `ui_feed_history()` (thời gian thực ngày). Mở rộng buffer nhãn trục thời gian lên 16 byte.
3. **Lỗi 3: Chuyển quyền sở hữu khi dữ liệu trùng nhau không đổi trục thời gian:**
   - *Hiện tượng:* Nạp ngoài dữ liệu trùng giá trị với dữ liệu tự thu thập khiến hàm thoát sớm trước khi tăng `s_history_rev`, trục giữ nguyên định dạng `+10m` thay vì `00:10`.
   - *Khắc phục:* Kiểm tra `!was_external` để đánh dấu `changed = true` khi chuyển giao quyền sở hữu từ tự động sang ngoài, buộc tăng `s_history_rev` và định dạng lại trục sang `00:10`. Các lần nạp dữ liệu ngoài tiếp theo giống hệt vẫn là no-op.

### Xử Lý Lỗi Dọn Tiến Trình Trên Windows (`tools/verify_production.ps1`)
- *Hiện tượng:* Script `verify_production.ps1` bị dừng đột ngột khi lệnh `taskkill.exe` trả về `Access denied` do quyền bảo mật tiến trình trên Windows.
- *Khắc phục:* Bọc lệnh dừng tiến trình và vòng lặp chờ trong khối `try { ... } catch {}` với cơ chế dự phòng `$proc.Kill()`, ngăn chặn script dừng bất thường. Xác nhận cả 4 pha verification chạy hoàn hảo và trả về mã thoát 0.

---

## 3. Thêm Mới 4 Bài Kiểm Thử Hồi Quy (`sim_pc/main.c`)

- **Check 36**: Kiểm tra mất kết nối 100 phút (P2 Lỗi 1) — kiểm tra chính xác 16 slot contiguity (85..100) và Min/Max hiển thị đúng điểm hợp lệ duy nhất (600 ppm).
- **Check 37**: Kiểm tra định dạng thời gian đơn điệu >24h (P2 Lỗi 2) — assert `+0m`, `+15m`, `+1h05m`, `+24h00m`, `+100h00m` và không bị cuộn về 0.
- **Check 38**: Kiểm tra chuyển quyền sở hữu ngoài khi giá trị trùng (P2 Lỗi 3) — assert đổi định dạng từ `+10m` sang `00:10` và tăng revision, lần nạp tiếp theo no-op.
- **Check 39**: Kiểm tra độ ổn định bộ nhớ khi chuyển đổi 100 lần liên tục (P1 Benchmark) — assert heap không rò rỉ và tái chọn thẻ no-op.

---

## 4. Bảng Mã Băm SHA-256 Các Tệp Đã Sửa Đổi & Xác Minh

```
Algorithm Hash                                                             Path
--------- ----                                                             ----
SHA256    E5C878CF74FEB0E3F0EABEF555006506082DB111A07858D70C8AC3057DCF745E ui.c
SHA256    D1130A23484A2E00462755BCD5B7A007C958CB1A3B1A51392E918CF00574B6E2 ui.h
SHA256    4EB81B4D9591B71DD8C327CEFF92B021431D00AB03324815F277DC67DD922D57 ui_types.h
SHA256    ECE426E57C5797DEF224D52C9183D1CFD8EB97E5EE3AC5386FED2CA2D25304ED sim_pc/main.c
SHA256    74B3DE6C717394B74B2F1FF2A4C7B7178A2F6F4F060123D5B70233B947C950AA sim_pc/ui_test_hooks.h
SHA256    9F848B407A83D2E4216E1E555E8634A7F40C03774F74EC38E128098462ADA435 tools/verify_production.ps1
```
