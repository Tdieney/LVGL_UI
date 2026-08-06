# UART Protocol Specification — UI Board & Motor Controller

> **`motor_comm_protocol.h`** (repository root) is the **single source of truth** for message framing and bitfield definitions (SOF `0xAA` / EOF `0x55`, 3 wire messages: `MotorCmd_t`, `MotorStatusFast_t`, `MotorStatusSlow_t`). This document details how the UI interacts with these structs. There are **no intermediate mapping structs** — the UI directly accesses the 3 global variables `motorCmd`, `motorStatusFast`, and `motorStatusSlow` (declared `extern` in `screens.h`, defined in `screens.c`).

The responsibility boundary remains strict: **The UI board requests commands** (by writing to `motorCmd`), while **the Motor board executes and reports status** (by populating `motorStatusFast` and `motorStatusSlow`). The UI never assumes command echo — displayed states reflect returned telemetry (see Section 3.1).

---

## 1. Frame Structure — `motor_comm_protocol.h`

```
| SOF (0xAA) | ID | LEN | PAYLOAD | EOF (0x55) |
```

Parsing uses fixed `LEN` per message ID (`MOTOR_CMD_LEN`, `MOTOR_STATUS_FAST_LEN`, `MOTOR_STATUS_SLOW_LEN`). Frame parsers must **not** search for `EOF` bytes alone, as payload bytes may coincidentally match `0x55`.

Each message is a C `union`: `.bits` accesses named bitfields, while `.bytes[]` provides raw wire serialization assuming 32-bit ARM little-endian alignment.

---

## 2. UI → Motor: `motorCmd` (`MotorCmd_t`, 6 Bytes, 100 ms Periodic)

`actions.c` updates global `motorCmd` directly upon user interaction. MCU firmware simply transmits this struct periodically:

| Wire Field | Description | Writer in `actions.c` |
|---|---|---|
| `cmd` (1 bit) | 0 = STOP, 1 = RUN | `action_motor_start`, `action_motor_stop` |
| `dir` (1 bit) | `MotorDirection_e` (FWD / REV) | `action_motor_dir_fwd`, `action_motor_dir_rev` |
| `opMode` (2 bits) | `OpMode_e` (SPEED, TORQUE, OPEN_LOOP, POSITION) | `action_mode_select` |
| `ctrlValRaw` (28 bits, unsigned) | Primary setpoint. OPEN_LOOP = PWM duty (0.1%/LSB), SPEED = RPM (1/LSB), TORQUE = mN·m (1/LSB), POSITION = **target angle (0.1°/LSB, 0..3600 = 0..360.0°)** | `action_motor_speed_change`, `action_torque_change`, `action_openloop_change`, `action_position_change` |
| `limitRaw` (16 bits, unsigned) | Secondary cascade limit. SPEED/OPEN_LOOP = **current limit** (1 mA/LSB), TORQUE/POSITION = **speed limit** (1 RPM/LSB) | `ctrl_limit_cb`, `ctrl_pos_limit_cb` |

---

### 2.1 Shared `ctrlValRaw` Handling

`ctrlValRaw` is shared across all 4 operating modes (`opMode`). UI widgets (sliders and readouts) evaluate `ctrlValRaw` ONLY when `motorCmd.bits.opMode` matches their respective mode, displaying 0 otherwise.

Furthermore, `action_mode_select()` resets both `ctrlValRaw` and `limitRaw` to 0 whenever `opMode` changes to prevent transmitting stale setpoints across different physical units.

---

### 2.2 Validated Torque & Position Units

- **TORQUE:** `UI_MAX_TORQUE_MNM` = 11000 (11 N·m maximum rated torque). The torque slider operates directly in mN·m (0..11000) and writes straight to `ctrlValRaw`.
- **POSITION:** `ctrlValRaw` represents absolute angle at 0.1°/LSB (0..3600 = 0..360.0°). The 360° ring knob maps 1:1 onto `ctrlValRaw` with 0° pointing at the top.

---

### 2.3 Cascade Limit (`limitRaw`)

The motor controller operates as a cascaded Position → Velocity → Torque control loop:
- **SPEED Mode:** `limitRaw` = Current Limit (mA)
- **TORQUE Mode:** `limitRaw` = Velocity Limit (RPM) — Runaway guard for unloaded torque commands
- **POSITION Mode:** `limitRaw` = Velocity Limit (RPM) — Slew rate limit for large step changes
- **OPEN_LOOP Mode:** `limitRaw` = Current Limit (mA)

---

### 2.4 RS-485 Configuration

`screens.h` exports global parameters `ui_rs485_baud`, `ui_rs485_parity`, and `ui_rs485_stopbits`. Settings dropdowns update these globals on user selection. Target firmware reads these values to reconfigure the RS-485 UART peripheral.

---

## 3. Motor → UI: Telemetry Data (`motorStatusFast` & `motorStatusSlow`)

UI tick routines read these structs directly and format values inline for display:

### 3.1 `motorStatusFast` (8 Bytes, 100 ms Telemetry)
- `actualRpm`: Motor velocity (RPM)
- `actualTorque`: Measured torque (0.1 N·m/LSB)
- `dir`: Current direction
- `motorState`: Motor state machine (STOPPED, STARTING, RUNNING, STOPPING, FAULT)
- `opMode`: Active operating mode confirmed by controller
- `current`: Bus current magnitude (mA)
- `voltage`: DC bus voltage (mV)
- `fault`: Fault bitfield byte (see Section 4)

### 3.2 `motorStatusSlow` (7 Bytes, 1000 ms Telemetry)
- `mtTemp`: Motor temperature (signed 0.1 °C/LSB)
- `invTemp`: Inverter/FET temperature (signed 0.1 °C/LSB)
- `efficiency`: System efficiency (0.1%/LSB)
- `pwmDuty`: Active PWM duty cycle (0.1%/LSB)

### 3.3 Telemetry Confirmation Principle
The UI waits for incoming `motorStatusFast` frames to update mode selection highlights and running state indicators. User interaction updates `motorCmd`, but visual feedback reflects confirmed controller telemetry.

---

## 4. Fault Bitfield Mapping

| Bit | Fault Description |
|---|---|
| 0 | Overcurrent (OC) |
| 1 | Overvoltage (OV) |
| 2 | Undervoltage (UV) |
| 3 | Over Temperature (OT) |
| 4 | Hall Sensor Fault |
| 5 | Encoder Fault |
| 6 | UART Link Timeout |
| 7 | Reserved |

The Diagnostics tab reads `motorStatusFast.bits.fault` directly to illuminate active fault indicators.

---

## 5. Offline Link Detection (`motorConnected`)

If the UI fails to receive a valid `motorStatusFast` frame within 500 ms, `motorConnected` is set to 0, switching the link status indicator to "OFFLINE".
