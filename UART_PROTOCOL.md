# UART Protocol Specification — UI Board & Motor Controller

> **`motor_comm.h`** (repository root) is the **single source of truth** for message framing and bitfield definitions (SOF `0xAA` / EOF `0x55`, 3 wire messages: `MotorCmd_t`, `MotorStatusFast_t`, `MotorStatusSlow_t`). This document details how the UI interacts with these structs. There are **no intermediate mapping structs** — the UI directly accesses the 3 global variables `motorCmd`, `motorStatusFast`, and `motorStatusSlow` (declared `extern` in `screens.h`, defined in `screens.c`).

The responsibility boundary remains strict: **The UI board requests commands** (by writing to `motorCmd`), while **the Motor board executes and reports status** (by populating `motorStatusFast` and `motorStatusSlow`). The UI never assumes command echo — displayed states reflect returned telemetry (see Section 3.1).

---

## 1. Frame Structure — `motor_comm.h`

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
| `opMode` (2 bits) | `OpMode_e`: SPEED=1, TORQUE=2, POSITION=3; value 0 is reserved | `ctrl_mode_dd_cb` |
| `ctrlValRaw` (28 bits, unsigned) | Primary setpoint. SPEED = RPM (1/LSB), TORQUE = target Iq (1 mA/LSB), POSITION = **target angle (0.1°/LSB, 0..3600 = 0..360.0°)** | `action_motor_speed_change`, `action_torque_change`, `action_position_change` |
| `limitRaw` (16 bits, unsigned) | Secondary cascade limit. SPEED/POSITION = **current limit** (1 mA/LSB), TORQUE = **speed limit** (1 RPM/LSB) | `ctrl_limit_cb`, `ctrl_pos_limit_cb` |

The HMI maps FWD to the CW icon and REV to the CCW icon, viewed from the
motor's output-shaft end. Installation/encoder polarity belongs in the motor
controller. `MotorStatusFast.dir` should report the measured logical direction,
not merely echo the last command bit.

---

### 2.1 Shared `ctrlValRaw` Handling

`ctrlValRaw` and `limitRaw` are shared across the 3 supported operating modes. UI widgets evaluate them only when `motorCmd.bits.opMode` matches their respective mode.

The Control UI keeps one process-lifetime target/limit pair per mode. Before a mode switch it saves the active pair; when returning to a mode it restores that mode's previous values. This avoids both unsafe cross-unit reinterpretation and the old reset-to-zero behavior.

---

### 2.2 Validated Iq & Position Units

- **TORQUE:** The mode name and numeric value remain for compatibility, but `ctrlValRaw` is target q-axis current in mA; this UI constrains it to 0..3000 (0..3.0 A).
- **POSITION:** `ctrlValRaw` represents absolute angle at 0.1°/LSB (0..3600 = 0..360.0°). The 360° ring knob maps 1:1 onto `ctrlValRaw` with 0° pointing at the top.

---

### 2.3 Cascade Limit (`limitRaw`)

The motor controller operates as a cascaded Position → Velocity → Iq-current control loop:
- **SPEED Mode:** `limitRaw` = Current Limit (mA)
- **TORQUE Mode:** `limitRaw` = Velocity Limit (RPM)
- **POSITION Mode:** `limitRaw` = Current Limit (mA)

---

### 2.4 RS-485 Configuration

`screens.h` exports pending parameters `ui_rs485_baud`, `ui_rs485_parity`, and
`ui_rs485_stopbits`. Dropdown changes and RESET update these RAM values only;
RESET restores 921600 / None / 1. SAVE calls the optional platform COMMIT hook.
Stop-bit codes are `0 = 1 bit` and `1 = 2 bits`; the former 1.5-bit choice is
retired. Reject or migrate older persisted records before returning them from
the LOAD hook.

Register the persistence hooks before `ui_init()`:

```c
static uint8_t rs485_load(ui_rs485_config_t *config)
{
    /* Read a versioned, CRC-checked record from Flash/NVS into config. */
    return platform_config_read(config, sizeof(*config));
}

static uint8_t rs485_commit(const ui_rs485_config_t *config)
{
    /* Apply/restart UART, then persist only after the new setting is valid. */
    if (!platform_rs485_apply(config->baud, config->parity, config->stopbits))
        return 0;
    return platform_config_write(config, sizeof(*config));
}

int main(void)
{
    ui_rs485_set_config_hooks(rs485_load, rs485_commit);
    ui_init();
    platform_rs485_apply(ui_rs485_baud, ui_rs485_parity, ui_rs485_stopbits);
    /* ... */
}
```

Without hooks, simulator and legacy builds remain RAM-only: SAVE reports success
but settings return to their compiled/seeded values after reboot. The target
record should include a magic value, schema version and CRC so erased/corrupt
Flash safely falls back to the factory defaults.

---

## 3. Motor → UI: Telemetry Data (`motorStatusFast` & `motorStatusSlow`)

UI tick routines read these structs directly and format values inline for display:

### 3.1 `motorStatusFast` (8 Bytes, 100 ms Telemetry)
- `actualRpm`: Motor velocity (RPM)
- `iqCurrent`: q-axis current magnitude (0.1 A/LSB)
- `dir`: Current direction
- `motorState`: Motor state machine (STOPPED, STARTING, RUNNING, STOPPING, FAULT)
- `opMode`: Active operating mode confirmed by controller
- `phaseCurrentRms`: Total phase RMS current magnitude (mA)
- `voltage`: DC bus voltage (mV)
- `fault`: Fault bitfield byte (see Section 4)

### 3.2 `motorStatusSlow` (7 Bytes, 1000 ms Telemetry)
- `mtTemp`: Motor temperature (signed 0.1 °C/LSB)
- `invTemp`: Inverter/FET temperature (signed 0.1 °C/LSB)
- `efficiency`: System efficiency (0.1%/LSB)
- `pwmDuty`: Active PWM duty cycle (0.1%/LSB)

### 3.3 Telemetry Confirmation Principle
The Control mode dropdown and its editor layout are command settings, so they
follow `motorCmd.bits.opMode` immediately even while disconnected. Actual speed,
Iq, Irms, direction, run state and faults continue to come from the selected
`motorStatusFast` source; the UI never fabricates feedback from the command.

### 3.4 UI Run Time

`RUN TIME` is UI-local and is not part of either UART status frame. It resets
when the UI accepts a START action, freezes on STOP or confirmed FAULT, and is
cleared by a device reboot. Because the timer is process-lifetime state rather
than screen state, switching tabs does not reset it.

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

The wire byte retains every fault bit for compatibility. The Diagnostics tab intentionally displays only OC, OV, UV, OT and UART Link; Hall Sensor and Encoder are omitted from the UI.

---

## 5. Offline Link Detection (`motorConnected`)

`motorConnected` is now a three-state UI-local value. Values 0 and 1 retain
their old meanings for source compatibility:

| Value | State | Color | Producer rule |
|---:|---|---|---|
| `UI_LINK_DISCONNECTED` (0) | DISCONNECTED | Red | RS-485 is disabled/unavailable, or hardware reports physical link loss |
| `UI_LINK_CONNECTED` (1) | CONNECTED | Green | Valid `motorStatusFast` frames are arriving |
| `UI_LINK_NO_RESPONSE` (2) | NO RESPONSE | Orange | RS-485 is enabled but no valid fast frame arrives within 500 ms |

Both the top link dot and Settings LINK STATUS row read the selected source via
`ui_motor_connected()`. If the target has no physical link-detect signal, it
should use NO RESPONSE for a silent bus and reserve DISCONNECTED for an
intentionally disabled/unconfigured interface.
