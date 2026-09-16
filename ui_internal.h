#ifndef UI_INTERNAL_H
#define UI_INTERNAL_H

#include <stdint.h>
#include <stdbool.h>
#include "ui_types.h"
#include "ui_auto.h"
#include "lora_comm.h"
#include "lora_hub_link.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Command phase enum:
 * UI_CMD_PHASE_IDLE: desired GPIO == reported GPIO (settled)
 * UI_CMD_PHASE_PENDING: desired != reported and age < UI_CMD_TIMEOUT_MS ("Sending…")
 * UI_CMD_PHASE_ERROR: desired != reported and age >= UI_CMD_TIMEOUT_MS ("Unknown" + "Retry")
 */
typedef enum {
    UI_CMD_PHASE_IDLE = 0,
    UI_CMD_PHASE_PENDING,
    UI_CMD_PHASE_ERROR
} ui_cmd_phase_t;

/* Internal command tracking per device */
typedef struct {
    ui_cmd_phase_t phase;
    uint32_t request_tick_ms; /* Timestamp when desired != reported */
    uint16_t seq;             /* Command sequence number dispatched by Hub */
} ui_device_cmd_state_t;

#ifdef __cplusplus
}
#endif

#endif /* UI_INTERNAL_H */
