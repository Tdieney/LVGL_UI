# Telemetry, giao thức wire và Demo mode

## Giao thức wire (`motor_comm.h`)

Frame: `SOF(0xAA) | ID | LEN | PAYLOAD | EOF(0x55)`. Parse theo LEN cố định, không
scan EOF (payload có thể chứa 0x55).

Các struct là **bitfield union**: `.bits` đặt tên trường, `.bytes[]` là wire bytes,
cùng vùng nhớ. Cần GCC/Keil/IAR little-endian bitfield convention (bit khai báo đầu
nằm ở bit thấp) — đã chốt ở 2 MCU.

| ID | Struct | Cadence | Nội dung |
|---|---|---|---|
| 0x11 | `MotorCmd_t` (6 byte wire) | HMI→Motor, 100 ms | cmd(1) dir(1) opMode(2) ctrlValRaw(28) limitRaw(16) |
| 0x22 | `MotorStatusFast_t` (8 byte) | Motor→HMI, 100 ms | actualRpm(10), iqCurrent(8, 0.1A/LSB), motorState(3), dir, opMode, phaseCurrentRms(16, mA), voltage(16, mV), fault(8) |
| 0x33 | `MotorStatusSlow_t` (7 byte) | Motor→HMI, 1000 ms | mtTemp(16, 0.1°C signed), invTemp(16), efficiency(10, 0.1%), pwmDuty(10, 0.1%) |

### Ý nghĩa `opMode` (không đổi số trên wire)

| Giá trị | Mode | ctrlValRaw | limitRaw |
|---|---|---|---|
| 1 | SPEED | target speed, 1 RPM/LSB | current limit, 1 mA/LSB |
| 2 | TORQUE | target Iq, 1 mA/LSB | speed limit, 1 RPM/LSB |
| 3 | POSITION | target angle, 0.1°/LSB (0..3600) | current limit, 1 mA/LSB |

`OPEN_LOOP = 0` đã ngừng nhưng **giữ giá trị số** (0 vẫn reserved). `dir` luôn là dấu
của ctrlValRaw. `limitRaw = 0` nghĩa là không có headroom — operator phải đặt limit.

### Fault bits (`fault` trong StatusFast)

`FAULT_OC(0) OV(1) UV(2) OT(3) HALL(4) ENCODER(5) COMM(6)`. UI hiển thị 5 bit
(OC/OV/UV/OT/COMM); HALL/ENCODER reserved nhưng ẩn khỏi Diagnostics.

## Lệnh Tx (`actions.c`)

- **START** chỉ đặt `cmd=1` khi `cmd` đang 0 và motor chưa chạy. Guard dựa **command
  state** trước, feedback sau: bấm START 2 lần trước khi status echo đầu tiên về thì
  lần 2 bị bỏ qua (không reset run-time origin).
- **STOP** cancel được cả lệnh START chưa được xác nhận. Trong cửa sổ
  START→echo-đầu-tiên (hoặc link câm) `motor_is_running()` có thể vẫn false trong khi
  `cmd` đã = 1; nếu guard chỉ dựa feedback thì lệnh START sẽ bị kẹt đến khi có status
  frame hợp lệ. STOP dùng `!cmd && !running` làm điều kiện return.
- Regression (`--stress-demo`) có check `offline_stop_bad`: bấm START rồi STOP ngay
  khi disconnected, `cmd` phải về 0.

## 2 nguồn telemetry và 3 accessor

| Nguồn | Struct | Khi nào dùng |
|---|---|---|
| UART thật | `motorStatusFast/Slow`, `motorConnected` | Firmware production |
| Demo sim | `s_demo_fast/s_demo_slow`, `s_demo_connected` (private trong `demo_sim.c`) | `demo_sim_active != 0` |

UI chỉ đọc qua `ui_motor_status_fast()`, `ui_motor_status_slow()`, `ui_motor_connected()`.
Demo không bao giờ ghi vào struct UART thật và ngược lại → UART receiver có thể chạy
song song với Demo mà không race (torn-read cũ không còn).

## Demo mode (`demo_sim.c`, chỉ compile khi `UI_DEMO_SIM=1`)

- Timer 100 ms, integer-only: SIN32 LUT 32 điểm (interp 16 bước) thay cho `sinf`,
  không `rand()`, temps/PWM/efficiency refresh ~1 Hz.
- **State machine level-driven** theo `motorCmd.cmd` (không edge-trigger): START giữa
  lúc STARTING/STOPPING không bị nuốt. Khởi động IDLE (không auto-start); operator
  bấm START. STARTING 1.6s, STOPPING 1.1s.
- TORQUE demo: tốc độ = speed-limit × Iq/ui ceiling. Dòng chạy theo load ramp để
  không nhảy tức thì lúc START.
- Demo OFF (chuyển về UART): buộc STOP + zero setpoint/limit để không mang trạng thái
  demo sang UART thật.
- Bật/tắt: giữ brand top bar ~800ms (chỉ build demo). Xem [SCREENS.md](SCREENS.md).

## Link state (UI-local, không phải wire field)

`ui_link_state_t`: `UI_LINK_DISCONNECTED(0) / CONNECTED(1) / NO_RESPONSE(2)`. UI vẽ
nhất quán ở top pill + Settings LINK STATUS: CONNECTED=xanh, NO RESPONSE=cam,
DISCONNECTED=đỏ.

## UI-local runtime timer

- `ui_runtime_on_start()`: reset + bắt đếm khi START được chấp nhận.
- `ui_runtime_on_stop()`: freeze giá trị.
- `ui_runtime_sync_state()`: theo motorState echo — FAULT xác nhận hoặc STOPPED
  với cmd=0 → freeze; STARTING/RUNNING mới bắt đầu đếm (hỗ trợ START ngoài nút UI).
- Clear tự nhiên khi reboot (BSS reset). Cap hiển thị 99999h.

## Giá trị UI-derived

- `motor_power_deciwatt()`: ước lượng P = V × Irms (deci-watt) — KHÔNG phải wire field.
- `motor_iq_deciamp()`: iqCurrent (0.1A/LSB) → deci-amp.
- `motor_is_running()`: motorState ∈ {STARTING, RUNNING}.
