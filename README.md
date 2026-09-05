# 7-inch Industrial HMI — BLDC Motor Control

LVGL v8.4 C UI cho màn hình công nghiệp 800x480 RGB565 (xSPI display có GRAM riêng),
tối ưu cho MCU tầm trung: 80 MHz, 128 KB SRAM, 1 MB Flash, heap LVGL 42 KB.

Thiết kế cốt lõi: **không cấp phát động trong tick loop, giữ phân mảnh heap thấp**;
caption tĩnh trỏ vào Flash, số động dùng buffer cố định, và mọi phép toán trong hot
path đều integer/fixed-point.

## Quick start (Windows)

```powershell
tools/build_sim.bat                # Build simulator PC (GDI)
tools/run_regression.bat           # Syntax + xSPI steady/switch + stress 120s + 6 shots
tools/shoot_all.bat                # Chụp 6 tab thành PNG + heap_summary.txt
sim_pc\build\sim_pc.exe            # Chạy simulator tương tác (chuột = cảm ứng)
```

Chi tiết: [docs/BUILD_TEST.md](docs/BUILD_TEST.md)

## Tài liệu

Hướng dẫn đọc tài liệu repo: [docs/README.md](docs/README.md) — bắt đầu từ đây.

| Chủ đề | File |
|---|---|
| Kiến trúc & luồng điều khiển | [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) |
| 6 màn hình + chrome dùng chung | [docs/SCREENS.md](docs/SCREENS.md) |
| Telemetry, giao thức wire, Demo mode | [docs/TELEMETRY.md](docs/TELEMETRY.md) |
| Hiệu năng & kỷ luật bộ nhớ | [docs/PERFORMANCE_MEMORY.md](docs/PERFORMANCE_MEMORY.md) |
| Build, test, công cụ developer | [docs/BUILD_TEST.md](docs/BUILD_TEST.md) |
| Hồ sơ heap đã kiểm chứng trên MCU | [docs/MCU_MEMORY_PROFILE.md](docs/MCU_MEMORY_PROFILE.md) |
| Nhật ký hoạt động (dev/AI) | [DEV_LOG.md](DEV_LOG.md) |

## Quy tắc chung

- Đọc [AGENTS.md](AGENTS.md) trước khi sửa UI; sau khi làm việc vật chất thì cập nhật
  `PLAN.md` và ghi `DEV_LOG.md`.
- Chạy `tools/run_regression.bat` trước khi commit.
