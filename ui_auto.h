/* =========================================================================
 * ui_auto.h
 *
 * Hub-local per-device automation rules & LCD-configurable thresholds (v1)
 *
 * Single source of truth for threshold boundaries, validation, qualification,
 * timing holds, and fault recovery. Zero LVGL dependencies.
 * ========================================================================= */

#ifndef UI_AUTO_H
#define UI_AUTO_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "ui_types.h"
#include "lora_comm.h"

#ifdef __cplusplus
extern "C" {
#endif

#define UI_AUTO_QUALIFY_FRAMES  3u
#define UI_AUTO_MIN_HOLD_MS     10000u

/* Canonical threshold limits and tap steps (per CURRENT.md R8) */
#define UI_AUTO_CO2_MIN         0
#define UI_AUTO_CO2_MAX         10000
#define UI_AUTO_CO2_STEP        50
#define UI_AUTO_CO2_MIN_GAP     50
#define UI_AUTO_CO2_DEF_ON      1000
#define UI_AUTO_CO2_DEF_OFF     800

#define UI_AUTO_VOC_MIN         1
#define UI_AUTO_VOC_MAX         500
#define UI_AUTO_VOC_STEP        5
#define UI_AUTO_VOC_MIN_GAP     5
#define UI_AUTO_VOC_DEF_ON      150
#define UI_AUTO_VOC_DEF_OFF     100

#define UI_AUTO_HUMID_MIN       0
#define UI_AUTO_HUMID_MAX       100
#define UI_AUTO_HUMID_STEP      1
#define UI_AUTO_HUMID_MIN_GAP   5
#define UI_AUTO_HUMID_DEF_ON    40
#define UI_AUTO_HUMID_DEF_OFF   50

typedef struct {
    const char *sensor_name;
    const char *unit_name;
    int32_t min_val;
    int32_t max_val;
    int32_t step;
    int32_t def_on;
    int32_t def_off;
    const char *on_op;   /* e.g. ">=" or "<=" */
    const char *off_op;  /* e.g. "<=" or ">=" */
} ui_auto_rule_info_t;

/* Internal runtime qualification state per device */
typedef struct {
    uint8_t qualify_count;       /* Consecutive eligible fresh frames */
    bool qualify_target_on;      /* Target logical output state being qualified */
    bool in_qualification;       /* Active qualification flag */
    uint32_t last_transition_ms; /* Timestamp of last confirmed transition or hold restart */
    bool paused_error;           /* Latched on command turnaround timeout */
    bool suspended_sensor;       /* Transient sensor/fault suspension (R5) */
} ui_auto_state_t;

/* Query whether a preset supports Auto rules in v1 */
bool ui_auto_is_preset_supported(uint8_t preset);

/* Get canonical rule info for a supported preset */
const ui_auto_rule_info_t *ui_auto_get_rule_info(uint8_t preset);

/* Validate ON/OFF thresholds for a preset */
bool ui_auto_validate_thresholds(uint8_t preset, int32_t on_val, int32_t off_val, char *err_msg, size_t err_sz);

/* Validate a complete hub device config */
bool ui_auto_validate_config(const ui_hub_device_config_t *cfg, char *err_msg, size_t err_sz);

/* Populate factory defaults for a preset */
void ui_auto_get_defaults(uint8_t idx, uint8_t preset, ui_hub_device_config_t *cfg);

/* Migrate legacy v1 config (without thresholds) safely to v2 */
bool ui_auto_migrate_legacy_config(const void *src, size_t src_sz, ui_hub_device_config_t *dst);

/* Step threshold value for a preset by its step size, clamping to allowed bounds */
void ui_auto_step_threshold(uint8_t preset, int32_t *thresh, int32_t dir);

/* Runtime state machine operations */
void ui_auto_state_init(ui_auto_state_t *state, uint32_t now);
void ui_auto_restart_hold(ui_auto_state_t *state, uint32_t now);
void ui_auto_rearm(ui_auto_state_t *state, uint32_t now);
void ui_auto_pause(ui_auto_state_t *state);
void ui_auto_reset_qualification(ui_auto_state_t *state);

/* Evaluate auto rule against incoming status frame (called strictly on fresh revisions).
 * Returns true if a qualified transition should be commanded, writing desired state to *desired_on_out. */
bool ui_auto_eval(ui_auto_state_t *state,
                  const ui_hub_device_config_t *cfg,
                  const lora_node_status_t *status,
                  bool link_online,
                  bool current_reported_on,
                  uint32_t now,
                  bool *desired_on_out);

#ifdef __cplusplus
}
#endif

#endif /* UI_AUTO_H */
