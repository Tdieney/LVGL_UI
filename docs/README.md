# Tài liệu dự án — Hướng dẫn đọc

Đây là chỉ mục tài liệu kỹ thuật cho developer. `README.md` ở root chỉ là phần giới
thiệu ngắn; mọi chi tiết nằm trong thư mục này.

## Chỉ mục

| File | Nội dung | Khi nào đọc |
|---|---|---|
| [ARCHITECTURE.md](ARCHITECTURE.md) | Kiến trúc tổng thể: SPA 1 màn hình, tab switching, update lanes, tách telemetry, luồng lệnh | Trước khi sửa `ui.c`, `actions.c` hoặc thêm cơ chế mới |
| [SCREENS.md](SCREENS.md) | Từng màn hình (Dashboard, Monitor, Control, Graphs, Diagnostics, Settings) + top bar/sidebar/splash | Trước khi sửa `screens.c` |
| [UI_COMPONENT_MAP.md](UI_COMPONENT_MAP.md) | Bảng quản lý từng component UI: widget code hay asset tĩnh, sửa ở đâu | Khi muốn đổi vị trí/hình ảnh của một thành phần |
| [TELEMETRY.md](TELEMETRY.md) | Giao thức wire (`motor_comm.h`), 2 nguồn telemetry + 3 accessor, Demo sim, runtime timer, trạng thái link | Trước khi sửa `demo_sim.c` hoặc protocol |
| [PERFORMANCE_MEMORY.md](PERFORMANCE_MEMORY.md) | Ràng buộc hiệu năng (xSPI, lane cadence) và kỷ luật heap (label pool, static text, shared styles, cấm float) | Trước khi thêm widget mới / đổi cấu trúc widget |
| [BUILD_TEST.md](BUILD_TEST.md) | Toàn bộ toolchain: build sim, regression, shot/layout dump, asset gen, export MCU | Trước khi build/test hoặc đổi tooling |
| [MCU_MEMORY_PROFILE.md](MCU_MEMORY_PROFILE.md) | Hồ sơ heap 42 KB đã kiểm chứng trên simulator + cấu hình firmware | Khi tích hợp sang firmware MCU |
| [screenshots/](screenshots/) | Ảnh tham chiếu 6 tab + Control/TORQUE/POSITION | So sánh trực quan trước/sau khi sửa |

## Nhật ký hoạt động: `DEV_LOG.md`

Mọi thay đổi vật chất (code, asset, tooling, quyết định thiết kế) phải được ghi vào
[`DEV_LOG.md`](../DEV_LOG.md) ở root repo — bởi **developer, hoặc AI** (bất kể công cụ
nào) thực hiện. Quy tắc:

- Mỗi mục: ngày, người/AI làm, phạm vi, tóm tắt, file chạm tới.
- Ghi ngắn gọn nhưng đủ để người khác biết "tại sao" (kèm tham chiếu PLAN.md / phase nếu có).
- Không thay thế `PLAN.md` (kế hoạch/trạng thái tính năng) — đây là lịch sử hoạt động.

## Thứ tự ưu tiên khi tài liệu mâu thuẫn

`AGENTS.md` + source code là nguồn chân lý. `CLAUDE.md` / `PERF.md` / `UART_PROTOCOL.md`
ở root là tài liệu giai đoạn cũ, giữ để tham khảo lịch sử — nếu chúng mâu thuẫn với
source hoặc AGENTS.md thì tin source và AGENTS.md, và nên cập nhật lại tài liệu cũ.

## Quy ước khi sửa tài liệu

- Tài liệu này mô tả hành vi **hiện tại** của source; khi sửa code hãy cập nhật cả
  tài liệu liên quan trong cùng lần thay đổi.
- Không viết duy trì 2 nơi cho 1 thông tin: trỏ chéo file thay vì copy nội dung.
