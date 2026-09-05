# Build, test & công cụ developer

Toàn bộ entrypoint là `.bat` trong `tools/`, mỗi cái launch PowerShell implementation
(process-local execution-policy bypass, không đổi máy). Các script resolve repo root
từ vị trí của chúng nên chạy từ đâu cũng được.

## Lệnh hằng ngày

```powershell
tools/build_sim.bat                        # Configure (CMake/Ninja) + build sim_pc.exe
tools/run_regression.bat                   # Full regression: syntax + position + stress + shots
tools/run_regression.bat -SkipBuild        # Dùng lại sim đã build
tools/shoot_all.bat                        # 6 tab -> PNG + heap_summary.txt (docs\screenshots)
tools/build_assets.bat --check             # Kiểm tra toolchain asset, không sinh lại
tools/export_mcu.bat --verify              # Kiểm tra manifest xuất MCU, không tạo zip
```

### `sim_pc.exe` — các chế độ chạy

| Mode | Mô tả |
|---|---|
| (mặc định) | Cửa sổ 800x480 tương tác, chuột = cảm ứng |
| `--shot <tab> <out.raw>` | Headless: render tab, advance virtual clock, dump RGB565 + in heap report |
| `--layout <tab>` | Headless: in cây widget dạng text (class, visibility, tọa độ, giá trị) |
| `--profile <tab>` | Đo pixel/callback flush ở steady state sau khi splash + tab đã settle |
| `--profile-switch <from> <to>` | Đo total + chrome frame + content burst của một lần đổi tab đã settle |
| `--stress-demo` | Demo stress 120s (mặc định) hoặc `--ms <n>` |
| `--test-nav` | Bơm pointer press/release thật vào tâm cả 6 hit-band và xác nhận đúng tab |
| `--test-dashboard-bands` | Kiểm tra SPEED arc: signal cyan <85%, amber ≥85%, danger khi fault/over-limit |

Chung: `--ms <n>` (1..600000 ms ảo; trước shot/layout mặc định 4000), `--demo` (demo
ON, motor STOPPED), `--demo-run` (demo ON + command mẫu khác 0), và
`--mode speed|torque|position` (mặc định speed; hữu ích khi review Control). Tab:
`dashboard|monitor|control|graphs|diagnostics|settings`.

- Raw → PNG: `python tools/raw2png.py out.raw out.png` (mặc định 800x480).
- `--layout` là "vision bằng text" cho layout debug: chỉ cần đọc class + x/y/w/h là bắt
  được overlap/clip/lệch offset; chạy được trong CI.
- `--profile control --mode position --demo-run --drag` mô phỏng một vòng kéo thật
  qua input driver. Report gồm tổng pixel, pixel/s, số refresh frame, burst lớn nhất
  và số callback sau khi draw buffer 10 dòng chia nhỏ transfer.
- `--profile-switch dashboard control --demo` đo hai refresh chrome/content; baseline
  full-screen cũ là 384k px. Regression khóa total ≤340k, chrome ≤22k, burst
  content ≤315k và bắt buộc đúng 2 frame.

## Regression (`run_regression.ps1`)

Thứ tự:
1. Syntax check `-Wall -Wextra -Werror` cho `screens.c/ui.c/actions.c/demo_sim.c` ở cả
   `UI_DEMO_SIM=0` và `1`.
2. Position test `tests/test_pos_angle.c` (integer atan2, góc wrap).
3. Headless layout check xác nhận đúng SPEED/TORQUE/POSITION và không còn subtree
   mode inactive bị hidden nhưng vẫn resident.
4. `--test-nav` đi qua input/event path thật của custom navigation và xác nhận đủ 6 tab.
5. `--test-dashboard-bands` drive SLOW tick bằng telemetry thật và khóa các ngưỡng
   169/170/201 RPM, fault và recovery.
6. Flush guard: Control SPEED/TORQUE ≤10k pixel/s + burst ≤16k; POSITION drag
   ≤120k pixel/s + burst ≤15k; Graphs rolling ≤360k pixel/s + burst ≤50k; đủ vòng
   sáu tab switch ≤340k px/lần.
7. `--stress-demo` (mặc định 120.000 ms) — kiểm tra heap min/biggest đều ≥8 KB + phân lập telemetry +
   rebuild 3 Control layouts.
8. 6 tab headless shots + heap report.

## Asset generation (`build_assets.bat`)

Chỉ chạy khi **cố ý** regenerate. Cần Python 3 + Pillow, Node/npx (`lv_font_conv`).
Sinh: logo splash, dashboard gauge face, icons Alpha-4, fonts
mono20/22/30/38/44/56/66 (JetBrains Mono Bold) + sans20/22 (Segoe UI Semibold cho
caption/button). Đầu ra `ui_icon_*.c`, `ui_img_*.c`, `fonts/*.c` là **firmware
assets** — review kích thước trước khi commit, giữ `const`.
Logo splash dùng source chuẩn nhỏ `tools/assets/ui_logo.png` (380x126, đã flatten lên
`COLOR_BG`) nên pipeline không còn phụ thuộc file thiết kế gốc đã bị dọn khỏi repo.

> **License font sans**: Segoe UI là font hệ thống Windows (proprietary) — thích hợp
> cho bản demo/nội bộ. Trước bản giao khách, thay `tools/ttf/SegoeUISemibold.ttf`
> bằng font OFL tương đương (vd Inter) rồi chạy lại `build_assets.bat`.

## MCU handoff

- `tools/export_mcu.bat [dest]`: gom source + fonts + assets + `ui_mcu_profile.h` +
  docs thành zip (và giải nén vào `dest/hmi_ui` nếu có dest). `--verify` chỉ kiểm tra.
- `tools/mcu_size.bat -Elf path/to/firmware.elf [-SizeTool ...]`: báo Flash (text+data)
  và static RAM (data+bss). Không thấy được stack peak — đo high-water trên hardware.

## Tooling tự động cho review UI

- `tools/shoot_all.bat` — một lệnh chụp cả 6 tab + `heap_summary.txt`. Options:
  `-Telemetry demo|demo-run|none`, `-OutDir`, `-SkipBuild`, `-Ms <n>`. Exit code
  non-zero khi có tab fail → dùng được cho pre-commit/CI.
- `sim_pc.exe --layout` kết hợp `--shot` cho loop: sửa → build → layout check →
  shoot → regression.

## Khi thêm/chỉnh tooling

- Thêm lệnh mới vào `tools/` (`.bat` wrapper + `.ps1` thật) và cập nhật file này +
  mục "Daily commands" ở đầu `tools/README.md`.
- Manifests (CMakeLists `UI_SOURCES`, `export_mcu.bat`, run_regression source list)
  là thủ công — đừng để chúng lệch nhau.
