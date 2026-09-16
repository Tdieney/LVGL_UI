/* =========================================================================
 * lora_comm.h  (bitfield + union version)
 *
 * LoRa peer-to-peer wire protocol between Smart Hub (LCD_Shield_LoRa)
 * and Smart Node (Relay_Shield_LoRa) carrying a Ra-01H module (SX127x).
 *
 * Single source of truth for everything exchanged between the two projects.
 *
 * Transport differences from UART:
 *   1) No SOF/EOF envelope: The SX127x in explicit-header mode already delivers
 *      a whole packet with internal length and CRC, so a start/end byte pair
 *      buys nothing and wastes airtime on a duty-cycle-limited radio link.
 *      A minimal header of { version, msg_id } is retained so mismatched builds
 *      are safely rejected.
 *   2) Single combined Node -> Hub status frame: Relay readback states and
 *      sensor telemetry are carried together in one periodic frame to minimize
 *      packet count, contention, and airtime overhead.
 *
 * Memory layout conventions:
 *   - Uses fixed-width types from <stdint.h> and <stdbool.h>.
 *   - Each message is a union: `.bits` gives named bitfield access,
 *     `.bytes[]` gives raw wire bytes -- zero manual packing/unpacking code.
 *   - Bitfields are declared low-bit-first.
 *   - Explicit little-endian assumption: assumes 32-bit ARM little-endian
 *     bitfield packing (GCC/Keil/IAR).
 *   - Scaled integers only; NO floating-point types anywhere.
 * ========================================================================= */

#ifndef LORA_COMM_H
#define LORA_COMM_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LORA_PROTO_VERSION      0x01u

#define LORA_MSG_HUB_CMD        0x01u
#define LORA_MSG_NODE_STATUS    0x02u

#define LORA_HUB_CMD_LEN        8u
#define LORA_NODE_STATUS_LEN    20u

/* Link staleness threshold: if no valid status frame arrives within this
 * interval, UI marks connection as Disconnected and hides stale live data. */
#ifndef LORA_LINK_TIMEOUT_MS
#define LORA_LINK_TIMEOUT_MS    5000u
#endif

/* Command turnaround timeout: if reported GPIO level differs from desired GPIO
 * level for longer than this duration, UI transitions from "Sending…" to "Unknown" + "Retry".
 * Rationale: accommodates 1 normal LoRa round-trip + 1 retry margin under SF7..SF10. */
#ifndef UI_CMD_TIMEOUT_MS
#define UI_CMD_TIMEOUT_MS       3000u
#endif

/* Fault flags reported by Node in lora_node_status_t */
typedef enum {
    LORA_FAULT_NONE         = 0,
    LORA_FAULT_RS485_BUS    = (1u << 0), /* RS-485 transceiver / bus communication failure */
    LORA_FAULT_CO2_TIMEOUT  = (1u << 1), /* MXVC10S-B-P67 sensor response timeout / offline */
    LORA_FAULT_TH_TIMEOUT   = (1u << 2)  /* TH10S-B-IP67 temp/humidity sensor timeout / offline */
} lora_fault_flags_e;

/* =========================================================================
 * 1) HUB -> NODE: lora_hub_cmd_t (8 bytes)
 *
 * Transmitted by Hub on relay state change, and re-transmitted periodically
 * so lost packets self-heal.
 *
 * Node applies raw GPIO levels verbatim (0 or 1) to PD16..PD13 without
 * knowing appliance semantics or polarity.
 * ========================================================================= */
typedef union {
    struct {
        /* Byte 0..1: Protocol Header */
        uint8_t  version;          /* LORA_PROTO_VERSION (0x01) */
        uint8_t  msg_id;           /* LORA_MSG_HUB_CMD (0x01) */

        /* Byte 2..3: Addressing placeholder */
        uint16_t node_addr;        /* Target node address (reserved for pairing/multi-node, default 0x0001) */

        /* Byte 4..5: Command Sequence */
        uint16_t seq;              /* Incrementing command sequence number */

        /* Byte 6: Desired Relay GPIO Levels (4 bits) */
        uint8_t  relay1_gpio : 1;  /* Desired level for Relay 1 (PD16): 0 = Low, 1 = High */
        uint8_t  relay2_gpio : 1;  /* Desired level for Relay 2 (PD15): 0 = Low, 1 = High */
        uint8_t  relay3_gpio : 1;  /* Desired level for Relay 3 (PD14): 0 = Low, 1 = High */
        uint8_t  relay4_gpio : 1;  /* Desired level for Relay 4 (PD13): 0 = Low, 1 = High */
        uint8_t  reserved_cmd: 4;  /* Reserved padding (always 0) */

        /* Byte 7: Alignment padding */
        uint8_t  reserved_pad;     /* Reserved padding (always 0) */
    } bits;
    uint8_t bytes[LORA_HUB_CMD_LEN];
} lora_hub_cmd_t;

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(sizeof(lora_hub_cmd_t) == LORA_HUB_CMD_LEN, "lora_hub_cmd_t wire size must be exactly 8 bytes");
#elif defined(__cplusplus) && __cplusplus >= 201103L
static_assert(sizeof(lora_hub_cmd_t) == LORA_HUB_CMD_LEN, "lora_hub_cmd_t wire size must be exactly 8 bytes");
#else
typedef char lora_assert_hub_cmd_sz[(sizeof(lora_hub_cmd_t) == LORA_HUB_CMD_LEN) ? 1 : -1];
#endif

/* =========================================================================
 * 2) NODE -> HUB: lora_node_status_t (20 bytes)
 *
 * Periodic status & telemetry broadcast from Node to Hub.
 * Contains readback GPIO levels, sequence echo, validity flags, fault flags,
 * node uptime, and environmental sensor readings.
 * ========================================================================= */
typedef union {
    struct {
        /* Byte 0..1: Protocol Header */
        uint8_t  version;          /* LORA_PROTO_VERSION (0x01) */
        uint8_t  msg_id;           /* LORA_MSG_NODE_STATUS (0x02) */

        /* Byte 2..3: Addressing placeholder */
        uint16_t node_addr;        /* Source node address (default 0x0001) */

        /* Byte 4..5: Sequence Echo */
        uint16_t seq_echo;         /* Last executed command sequence number echoed back */

        /* Byte 6: Actual Relay GPIO Levels (bits 0..3) & Metric Validity (bits 4..7) */
        uint8_t  relay1_gpio : 1;  /* Actual readback level of Relay 1 (PD16) */
        uint8_t  relay2_gpio : 1;  /* Actual readback level of Relay 2 (PD15) */
        uint8_t  relay3_gpio : 1;  /* Actual readback level of Relay 3 (PD14) */
        uint8_t  relay4_gpio : 1;  /* Actual readback level of Relay 4 (PD13) */
        uint8_t  co2_valid   : 1;  /* 1 = CO2 reading valid, 0 = invalid / warm-up / fault */
        uint8_t  voc_valid   : 1;  /* 1 = VOC reading valid, 0 = invalid / warm-up / fault */
        uint8_t  temp_valid  : 1;  /* 1 = Temp reading valid, 0 = invalid / sensor fault */
        uint8_t  humid_valid : 1;  /* 1 = Humidity reading valid, 0 = invalid / fault */

        /* Byte 7: Fault Flags */
        uint8_t  rs485_fault     : 1; /* RS-485 bus fault */
        uint8_t  co2_timeout     : 1; /* CO2/VOC sensor communication timeout */
        uint8_t  th_timeout      : 1; /* Temp/Humid sensor communication timeout */
        uint8_t  reserved_faults : 5; /* Reserved padding */

        /* Byte 8..11: Node Uptime */
        uint32_t uptime_sec;       /* Node operational uptime in seconds */

        /* Byte 12..13: CO2 Reading */
        uint16_t co2_ppm;          /* CO2 concentration in ppm (0..10000) */

        /* Byte 14..15: VOC Reading */
        uint16_t voc_index;        /* TVOC unitless index (0..500) */

        /* Byte 16..17: Temperature Reading */
        int16_t  temp_deci_c;      /* Signed temperature in 0.1 °C (e.g. 235 = +23.5 °C, -25 = -2.5 °C) */

        /* Byte 18..19: Humidity & Padding */
        uint8_t  humidity_pct;     /* Relative humidity in whole percent (0..100 %) */
        uint8_t  reserved_pad;     /* Reserved padding (always 0) */

        /* Link quality is deliberately NOT carried on the wire.
         *
         * RSSI and SNR are measured by whichever radio RECEIVES a frame, so the
         * Hub already learns the downlink quality (Hub hearing Node) locally from
         * its own SX127x -- see lora_hub_link.h. That is what drives the signal
         * indicator, and it uses SNR rather than RSSI: LoRa demodulates below the
         * noise floor, so quality is the margin of SNR above the demodulator limit
         * for the chosen spreading factor, not the absolute RSSI.
         *
         * The reverse direction (Node hearing Hub) can only be measured by the
         * Node and would need a field here. It is omitted on purpose: when the
         * Node cannot hear the Hub, commands go unconfirmed and the UI already
         * reports that per device as Unknown + Retry, which is more actionable
         * than a header icon. Add int8_t hub_rssi_dbm / hub_snr_db here only if
         * commissioning genuinely needs uplink diagnostics. */
    } bits;
    uint8_t bytes[LORA_NODE_STATUS_LEN];
} lora_node_status_t;

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(sizeof(lora_node_status_t) == LORA_NODE_STATUS_LEN, "lora_node_status_t wire size must be exactly 20 bytes");
#elif defined(__cplusplus) && __cplusplus >= 201103L
static_assert(sizeof(lora_node_status_t) == LORA_NODE_STATUS_LEN, "lora_node_status_t wire size must be exactly 20 bytes");
#else
typedef char lora_assert_node_status_sz[(sizeof(lora_node_status_t) == LORA_NODE_STATUS_LEN) ? 1 : -1];
#endif

static inline void lora_hub_cmd_init_header(lora_hub_cmd_t *cmd)
{
    cmd->bits.version = LORA_PROTO_VERSION;
    cmd->bits.msg_id = LORA_MSG_HUB_CMD;
    cmd->bits.node_addr = 0x0001u;
}

static inline void lora_node_status_init_header(lora_node_status_t *status)
{
    status->bits.version = LORA_PROTO_VERSION;
    status->bits.msg_id = LORA_MSG_NODE_STATUS;
    status->bits.node_addr = 0x0001u;
}

static inline uint8_t lora_hub_cmd_get_relay_gpio(const lora_hub_cmd_t *cmd, uint8_t idx)
{
    switch (idx) {
        case 0: return cmd->bits.relay1_gpio;
        case 1: return cmd->bits.relay2_gpio;
        case 2: return cmd->bits.relay3_gpio;
        case 3: return cmd->bits.relay4_gpio;
        default: return 0;
    }
}

static inline void lora_hub_cmd_set_relay_gpio(lora_hub_cmd_t *cmd, uint8_t idx, uint8_t level)
{
    uint8_t v = (level != 0) ? 1u : 0u;
    switch (idx) {
        case 0: cmd->bits.relay1_gpio = v; break;
        case 1: cmd->bits.relay2_gpio = v; break;
        case 2: cmd->bits.relay3_gpio = v; break;
        case 3: cmd->bits.relay4_gpio = v; break;
        default: break;
    }
}

static inline uint8_t lora_node_status_get_relay_gpio(const lora_node_status_t *status, uint8_t idx)
{
    switch (idx) {
        case 0: return status->bits.relay1_gpio;
        case 1: return status->bits.relay2_gpio;
        case 2: return status->bits.relay3_gpio;
        case 3: return status->bits.relay4_gpio;
        default: return 0;
    }
}

static inline void lora_node_status_set_relay_gpio(lora_node_status_t *status, uint8_t idx, uint8_t level)
{
    uint8_t v = (level != 0) ? 1u : 0u;
    switch (idx) {
        case 0: status->bits.relay1_gpio = v; break;
        case 1: status->bits.relay2_gpio = v; break;
        case 2: status->bits.relay3_gpio = v; break;
        case 3: status->bits.relay4_gpio = v; break;
        default: break;
    }
}

static inline uint8_t lora_hub_cmd_get_relay_gpios_mask(const lora_hub_cmd_t *cmd)
{
    return (uint8_t)(cmd->bits.relay1_gpio | (cmd->bits.relay2_gpio << 1) |
                     (cmd->bits.relay3_gpio << 2) | (cmd->bits.relay4_gpio << 3));
}

static inline void lora_hub_cmd_set_relay_gpios_mask(lora_hub_cmd_t *cmd, uint8_t mask)
{
    cmd->bits.relay1_gpio = (mask >> 0) & 1u;
    cmd->bits.relay2_gpio = (mask >> 1) & 1u;
    cmd->bits.relay3_gpio = (mask >> 2) & 1u;
    cmd->bits.relay4_gpio = (mask >> 3) & 1u;
}

static inline uint8_t lora_node_status_get_relay_gpios_mask(const lora_node_status_t *status)
{
    return (uint8_t)(status->bits.relay1_gpio | (status->bits.relay2_gpio << 1) |
                     (status->bits.relay3_gpio << 2) | (status->bits.relay4_gpio << 3));
}

static inline void lora_node_status_set_relay_gpios_mask(lora_node_status_t *status, uint8_t mask)
{
    status->bits.relay1_gpio = (mask >> 0) & 1u;
    status->bits.relay2_gpio = (mask >> 1) & 1u;
    status->bits.relay3_gpio = (mask >> 2) & 1u;
    status->bits.relay4_gpio = (mask >> 3) & 1u;
}

/* =========================================================================
 * Wire Globals (Shared State Contract)
 *
 * Declared extern here, defined in the application layer (or sim_pc),
 * and accessed directly by the UI.
 * ========================================================================= */
extern lora_hub_cmd_t     lora_hub_cmd;
extern lora_node_status_t  lora_node_status;

#ifdef __cplusplus
}
#endif

#endif /* LORA_COMM_H */
