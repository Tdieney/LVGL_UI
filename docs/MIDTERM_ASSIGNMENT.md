# BÀI TẬP GIỮA KHÓA: SMART STUDIO UI

## 1. Yêu cầu đề bài

Vibe code một giao diện HMI điều khiển **Căn hộ Studio thông minh** bằng **LVGL v8.4** trên ESP32-S3 và màn hình cảm ứng:
* **Không cần phần cứng thật** (không cần relay, đèn, máy lạnh thật).
* **Dùng PC làm thiết bị ảo (Digital Twin)**: Viết hoặc dùng một trang HTML kết nối trực tiếp cổng COM qua Web Serial API.
* **Đồng bộ 2 chiều thời gian thực**:
  - Có thể thay đổi baudrate trên Web Serial (ESP32-S3) và trên Web Serial (PC) trong khi đang chạy.

---

## 2. Các thiết bị cần điều khiển

1. **💡 Đèn chiếu sáng chính**:
   - 1 Switch (Bật / Tắt).
   - 1 Slider điều chỉnh độ sáng Dimmer (`0% - 100%`).
2. **❄️ Điều hòa không khí (AC)**:
   - 1 Switch (Bật / Tắt).
   - Chỉnh nhiệt độ từ `16°C` đến `30°C` (nút `+`/`-` hoặc slider/arc).
3. **🌡️ Môi trường phòng**:
   - Hiển thị nhiệt độ (°C) và độ ẩm (%) nhận từ Web gửi sang.
4. **🏠 Kịch bản chuyển cảnh**:
   - Nút bấm cảnh nhanh: *Về nhà* (Bật đèn, máy lạnh 24°C) / *Đi ngủ* (Tắt đèn, máy lạnh 26°C) / *Rời nhà* (Tắt hết).

---

## 3. Giao thức UART đề xuất (JSON + `\n`)

Giao tiếp qua cổng USB-UART, mỗi gói tin là chuỗi JSON kết thúc bằng `\n`. Tất cả thông số số (`power`, `bright`, `temp`, `humid`) đều dùng **số nguyên** (`int`), không dùng số thập phân:

| Hướng | Mục đích | Ví dụ gói tin JSON |
| :--- | :--- | :--- |
| **Web $\rightarrow$ ESP32** | Chỉnh đèn | `{"cmd":"LIGHT", "power":1, "bright":80}\n` |
| **Web $\rightarrow$ ESP32** | Chỉnh điều hòa | `{"cmd":"AC", "power":1, "temp":25}\n` |
| **Web $\rightarrow$ ESP32** | Bắn nhiệt ẩm ảo | `{"cmd":"ENV", "temp":27, "humid":60}\n` |
| **ESP32 $\rightarrow$ Web** | Chạm đổi đèn trên LCD | `{"event":"LIGHT", "power":1, "bright":80}\n` |
| **ESP32 $\rightarrow$ Web** | Đổi nhiệt độ trên LCD | `{"event":"AC", "power":1, "temp":25}\n` |

Tham khảo file giao diện Web mẫu [web_serial_starter.html](./web_serial_starter.html)

---

## 4. Nộp bài

* Quay một video ngắn (**30 giây - 1 phút**) gửi vào group lớp:
  - Thấy rõ màn hình cảm ứng và màn hình máy tính.
  - Thao tác: Kéo/bật trên LCD thấy Web nhảy theo; Kéo/bật trên Web thấy LCD nhảy theo.
