/* ============================================================
 * motor_comm_protocol.h  (bitfield + union version)
 *
 * UART wire protocol between HMI MCU and Motor MCU for a BLDC FOC
 * motor control system (GIM6010-8, 24V).
 *
 * Frame envelope: | SOF(0xAA) | ID | LEN | PAYLOAD | EOF(0x55) |
 * Parse using LEN (fixed per ID), not by scanning for EOF -- a
 * payload byte can legitimately equal 0x55.
 *
 * Each message below is a union: `.bits` gives named field access,
 * `.bytes[]` gives the raw wire bytes -- same memory, no manual
 * pack/unpack code. This relies on 2 things holding true:
 *
 *   1) Both MCUs compile with the same ARM little-endian bitfield
 *      convention (GCC/Keil/IAR all agree here): the FIRST declared
 *      bitfield member occupies the LOWEST bits. Field declaration
 *      order below is written low-bit-first to match the spec sheet.
 *
 *   2) Wire byte order is NATIVE little-endian: bytes[0] is the LSB
 *      byte and is sent FIRST. This is the reverse of the sheet's
 *      left-to-right "Byte 3..0" / "Byte 6..0" reading order (which
 *      was written MSB-first) -- reusing the union's raw memory
 *      directly is what makes this approach simple; forcing
 *      MSB-first on the wire again would need manual byte-swap code.
 * ============================================================ */

#ifndef MOTOR_COMM_PROTOCOL_H
#define MOTOR_COMM_PROTOCOL_H

#include <stdint.h>

#define PROTO_SOF 0xAAu
#define PROTO_EOF 0x55u

#define MOTOR_CMD_LEN         6u
#define MOTOR_STATUS_FAST_LEN 8u
#define MOTOR_STATUS_SLOW_LEN 7u

typedef enum
{
    MSG_ID_MOTOR_CMD         = 0x11,
    MSG_ID_MOTOR_STATUS_FAST = 0x22,
    MSG_ID_MOTOR_STATUS_SLOW = 0x33
} MsgId_e;

/* Fault bits packed into the 8-bit FAULT field of MOTOR_STATUS_FAST */
typedef enum
{
    FAULT_OC      = (1u << 0), // Over-current
    FAULT_OV      = (1u << 1), // Over-voltage
    FAULT_UV      = (1u << 2), // Under-voltage
    FAULT_OT      = (1u << 3), // Over-temperature
    FAULT_HALL    = (1u << 4), // Hall sensor fault
    FAULT_ENCODER = (1u << 5), // Encoder fault
    FAULT_COMM    = (1u << 6)  // UART link fault (receive timeout)
    /* bit 7 reserved */
} FaultBit_e;

/* DIR is always the sign of ctrlValRaw, for every opMode:
 * dir = 0 (FWD) -> value = +ctrlValRaw, dir = 1 (REV) -> value = -ctrlValRaw */
typedef enum { MOTOR_DIR_FWD = 0, MOTOR_DIR_REV = 1 } MotorDirection_e;

/* ctrlValRaw is the PRIMARY setpoint; limitRaw (added below) is the SECONDARY
 * safety limit the FOC cascade always applies underneath the primary command
 * (this controller family is ODrive-derived: Position->Velocity->Torque). Both
 * fields' meaning depend on opMode: */
typedef enum
{
    OP_MODE_OPEN_LOOP = 0, // ctrlValRaw = PWM duty, 0.1 %/LSB (0..1000);  limitRaw = current limit, 1 mA/LSB
    OP_MODE_SPEED     = 1, // ctrlValRaw = target speed, 1 RPM/LSB;        limitRaw = current limit, 1 mA/LSB
    OP_MODE_TORQUE    = 2, // ctrlValRaw = target torque, 1 mN.m/LSB;      limitRaw = speed limit,   1 RPM/LSB
    OP_MODE_POSITION  = 3  // ctrlValRaw = target angle, 0.1 deg/LSB;      limitRaw = speed limit,   1 RPM/LSB
    // limitRaw's speed limit (TORQUE/POSITION) is the runaway guard: a torque
    // command under no load, or a large position step, is bled off as the
    // shaft speed approaches this ceiling. The current limit (SPEED/OPEN_LOOP)
    // caps stator current to protect the motor/FETs. limitRaw == 0 means "no
    // headroom" (motor won't move) — the operator must dial in a limit too.
} OpMode_e;

typedef enum
{
    MOTOR_STATE_STOPPED  = 0,
    MOTOR_STATE_STARTING = 1,
    MOTOR_STATE_RUNNING  = 2,
    MOTOR_STATE_STOPPING = 3,
    MOTOR_STATE_FAULT    = 4
} MotorState_e;

/* ============================================================
 * 1) MOTOR_CMD -- HMI MCU -> Motor MCU, 6 bytes, periodic 100 ms
 *
 * Container is uint64_t (like the two status structs) so the extra limitRaw
 * field stays in one storage unit -- only bytes[0..5] are the wire frame,
 * always send exactly MOTOR_CMD_LEN (6) bytes, never sizeof(bytes) (that's 8).
 *
 * Example:
 *   MotorCmd_t cmd = {0};
 *   cmd.bits.cmd        = 1;
 *   cmd.bits.dir        = MOTOR_DIR_FWD;
 *   cmd.bits.opMode     = OP_MODE_SPEED;
 *   cmd.bits.ctrlValRaw = 1500;               // 1500 rpm
 *   cmd.bits.limitRaw   = 12000;              // current limit 12.0 A (12000 mA)
 *   UART_Send(cmd.bytes, MOTOR_CMD_LEN);
 * ============================================================ */
typedef union
{
    struct
    {
        uint64_t cmd        : 1;  // bit0:     0 STOP, 1 RUN
        uint64_t dir        : 1;  // bit1:     MotorDirection_e
        uint64_t opMode     : 2;  // bit3:2:   OpMode_e
        uint64_t ctrlValRaw : 28; // bit31:4:  unsigned PRIMARY setpoint, meaning per opMode
        uint64_t limitRaw   : 16; // bit47:32: unsigned SECONDARY limit, meaning per opMode (see OpMode_e)
        uint64_t reserved   : 16; // bit63:48: unused padding -- always 0, not on the wire
    } bits;
    uint64_t raw;
    uint8_t  bytes[8]; // only bytes[0..5] are the wire frame; bytes[6..7] unused padding
} MotorCmd_t;

static inline int32_t MotorCmd_GetSignedCtrlVal(const MotorCmd_t *cmd)
{
    return (cmd->bits.dir == MOTOR_DIR_REV) ? -(int32_t)cmd->bits.ctrlValRaw
                                             :  (int32_t)cmd->bits.ctrlValRaw;
}

/* ============================================================
 * 2) MOTOR_STATUS_FAST -- Motor MCU -> HMI MCU, 8 bytes, periodic 100 ms
 *
 * Container is uint64_t so the compiler keeps every field in one
 * storage unit -- all 8 bytes (bytes[0..7]) are now the wire frame,
 * always send exactly MOTOR_STATUS_FAST_LEN bytes.
 *
 * v3: added `actualTorque` right after `actualRpm` (feedback pair
 * speed+torque for the Control screen). 0.1 N.m/LSB, so 8 bits cover
 * 0..25.5 N.m (110 = 11.0 N.m = the GIM6010-8's peak). This filled the
 * one previously-spare byte, so the frame grew 7 -> 8 bytes.
 * ============================================================ */
typedef union
{
    struct
    {
        uint64_t actualRpm      : 10; // bit9:0,   0..1023, output-shaft speed (rpm)
        uint64_t actualTorque   : 8;  // bit17:10, output torque magnitude, 0.1 N.m/LSB (0..25.5)
        uint64_t motorState     : 3;  // bit20:18, MotorState_e
        uint64_t dir            : 1;  // bit21,    actual direction feedback
        uint64_t opMode         : 2;  // bit23:22, OpMode_e, echoed back
        uint64_t current        : 16; // bit39:24, stator current magnitude (mA)
        uint64_t voltage        : 16; // bit55:40, DC bus voltage magnitude (mV)
        uint64_t fault          : 8;  // bit63:56, see FaultBit_e
    } bits;
    uint64_t raw;
    uint8_t  bytes[8]; // all 8 bytes are the wire frame now
} MotorStatusFast_t;

/* ============================================================
 * 3) MOTOR_STATUS_SLOW -- Motor MCU -> HMI MCU, 7 bytes, periodic 1000 ms
 *
 * Same uint64_t container note as MOTOR_STATUS_FAST above.
 * mtTemp/invTemp are stored as raw unsigned bits -- use the
 * getters below to read them back as signed 0.1 degC values.
 * bits55:52 are unused by the spec sheet (only 52 of 56 bits are
 * assigned); kept here as `reserved`, always written/read as 0.
 * ============================================================ */
typedef union
{
    struct
    {
        uint64_t mtTemp        : 16; // bit15:0,  raw bits of a signed 0.1 degC value
        uint64_t invTemp       : 16; // bit31:16, raw bits of a signed 0.1 degC value
        uint64_t efficiency    : 10; // bit41:32, 0.1 %/LSB
        uint64_t pwmDuty       : 10; // bit51:42, 0.1 %/LSB
        uint64_t reserved         : 4;  // bit55:52, unused -- always 0
    } bits;
    uint64_t raw;
    uint8_t  bytes[8]; // only bytes[0..6] are the wire frame; bytes[7] unused padding
} MotorStatusSlow_t;

static inline int16_t MotorStatusSlow_GetMtTemp(const MotorStatusSlow_t *s)
{
    return (int16_t)s->bits.mtTemp;
}

static inline int16_t MotorStatusSlow_GetInvTemp(const MotorStatusSlow_t *s)
{
    return (int16_t)s->bits.invTemp;
}

#endif /* MOTOR_COMM_PROTOCOL_H */
