/* =========================================================================
 * ui_auto.c
 *
 * Hub-local per-device automation rules & LCD-configurable thresholds (v1)
 * ========================================================================= */

#include "ui_auto.h"
#include <stdio.h>
#include <string.h>

static const ui_auto_rule_info_t s_rules[] = {
    [UI_PRESET_PURIFIER] = {
        .sensor_name = "VOC",
        .unit_name = "",
        .min_val = UI_AUTO_VOC_MIN,
        .max_val = UI_AUTO_VOC_MAX,
        .step = UI_AUTO_VOC_STEP,
        .def_on = UI_AUTO_VOC_DEF_ON,
        .def_off = UI_AUTO_VOC_DEF_OFF,
        .on_op = ">=",
        .off_op = "<="
    },
    [UI_PRESET_FAN] = {
        .sensor_name = "CO2",
        .unit_name = "ppm",
        .min_val = UI_AUTO_CO2_MIN,
        .max_val = UI_AUTO_CO2_MAX,
        .step = UI_AUTO_CO2_STEP,
        .def_on = UI_AUTO_CO2_DEF_ON,
        .def_off = UI_AUTO_CO2_DEF_OFF,
        .on_op = ">=",
        .off_op = "<="
    },
    [UI_PRESET_HUMIDIFIER] = {
        .sensor_name = "Humidity",
        .unit_name = "%RH",
        .min_val = UI_AUTO_HUMID_MIN,
        .max_val = UI_AUTO_HUMID_MAX,
        .step = UI_AUTO_HUMID_STEP,
        .def_on = UI_AUTO_HUMID_DEF_ON,
        .def_off = UI_AUTO_HUMID_DEF_OFF,
        .on_op = "<=",
        .off_op = ">="
    }
};

static const char *s_preset_default_names[UI_PRESET_COUNT] = {
    "Air Purifier",
    "Ventilation Fan",
    "Humidifier",
    "Dehumidifier",
    "Heater",
    "Desk Light",
    "Smart Socket",
    "Generic Device"
};

bool ui_auto_is_preset_supported(uint8_t preset)
{
    return (preset == UI_PRESET_PURIFIER ||
            preset == UI_PRESET_FAN ||
            preset == UI_PRESET_HUMIDIFIER);
}

const ui_auto_rule_info_t *ui_auto_get_rule_info(uint8_t preset)
{
    if (!ui_auto_is_preset_supported(preset)) return NULL;
    return &s_rules[preset];
}

bool ui_auto_validate_thresholds(uint8_t preset, int32_t on_val, int32_t off_val, char *err_msg, size_t err_sz)
{
    if (!ui_auto_is_preset_supported(preset)) {
        if (err_msg && err_sz > 0) {
            snprintf(err_msg, err_sz, "Manual only for this preset");
        }
        return false;
    }
    const ui_auto_rule_info_t *rule = &s_rules[preset];

    if (on_val < rule->min_val || on_val > rule->max_val) {
        if (err_msg && err_sz > 0) {
            snprintf(err_msg, err_sz, "On limit out of bounds (%d..%d)", (int)rule->min_val, (int)rule->max_val);
        }
        return false;
    }
    if (off_val < rule->min_val || off_val > rule->max_val) {
        if (err_msg && err_sz > 0) {
            snprintf(err_msg, err_sz, "Off limit out of bounds (%d..%d)", (int)rule->min_val, (int)rule->max_val);
        }
        return false;
    }

    if (preset == UI_PRESET_FAN) {
        if (on_val < off_val + UI_AUTO_CO2_MIN_GAP) {
            if (err_msg && err_sz > 0) {
                snprintf(err_msg, err_sz, "ON must exceed OFF by at least %d ppm", UI_AUTO_CO2_MIN_GAP);
            }
            return false;
        }
    } else if (preset == UI_PRESET_PURIFIER) {
        if (on_val < off_val + UI_AUTO_VOC_MIN_GAP) {
            if (err_msg && err_sz > 0) {
                snprintf(err_msg, err_sz, "ON must exceed OFF by at least %d", UI_AUTO_VOC_MIN_GAP);
            }
            return false;
        }
    } else if (preset == UI_PRESET_HUMIDIFIER) {
        if (off_val < on_val + UI_AUTO_HUMID_MIN_GAP) {
            if (err_msg && err_sz > 0) {
                snprintf(err_msg, err_sz, "OFF must exceed ON by at least %d %%RH", UI_AUTO_HUMID_MIN_GAP);
            }
            return false;
        }
    }

    if (err_msg && err_sz > 0) err_msg[0] = '\0';
    return true;
}

bool ui_auto_validate_config(const ui_hub_device_config_t *cfg, char *err_msg, size_t err_sz)
{
    if (cfg == NULL) {
        if (err_msg && err_sz > 0) snprintf(err_msg, err_sz, "Config is null");
        return false;
    }
    if (cfg->schema_version != UI_CONFIG_SCHEMA_VERSION) {
        if (err_msg && err_sz > 0) snprintf(err_msg, err_sz, "Unsupported schema version 0x%02X", cfg->schema_version);
        return false;
    }
    if (cfg->preset >= UI_PRESET_COUNT) {
        if (err_msg && err_sz > 0) snprintf(err_msg, err_sz, "Invalid preset ID %u", cfg->preset);
        return false;
    }
    if (cfg->active_low > 1u) {
        if (err_msg && err_sz > 0) snprintf(err_msg, err_sz, "Invalid active_low flag %u", cfg->active_low);
        return false;
    }
    if (cfg->mode != UI_MODE_AUTO && cfg->mode != UI_MODE_MANUAL) {
        if (err_msg && err_sz > 0) snprintf(err_msg, err_sz, "Invalid mode %u", (unsigned)cfg->mode);
        return false;
    }
    /* Ensure null termination within name buffer */
    bool terminated = false;
    for (size_t i = 0; i < sizeof(cfg->name); i++) {
        if (cfg->name[i] == '\0') {
            terminated = true;
            break;
        }
    }
    if (!terminated || cfg->name[0] == '\0') {
        if (err_msg && err_sz > 0) snprintf(err_msg, err_sz, "Invalid device name");
        return false;
    }

    if (!ui_auto_is_preset_supported(cfg->preset)) {
        if (cfg->mode == UI_MODE_AUTO) {
            if (err_msg && err_sz > 0) snprintf(err_msg, err_sz, "Manual only for %s", s_preset_default_names[cfg->preset]);
            return false;
        }
    } else {
        if (!ui_auto_validate_thresholds(cfg->preset, cfg->auto_on_thresh, cfg->auto_off_thresh, err_msg, err_sz)) {
            return false;
        }
    }

    if (err_msg && err_sz > 0) err_msg[0] = '\0';
    return true;
}

void ui_auto_get_defaults(uint8_t idx, uint8_t preset, ui_hub_device_config_t *cfg)
{
    if (cfg == NULL) return;
    if (preset >= UI_PRESET_COUNT) preset = UI_PRESET_GENERIC;

    memset(cfg, 0, sizeof(*cfg));
    cfg->preset = preset;
    snprintf(cfg->name, sizeof(cfg->name), "%s", s_preset_default_names[preset]);
    cfg->mode = UI_MODE_MANUAL;
    cfg->active_low = 0u;
    cfg->schema_version = UI_CONFIG_SCHEMA_VERSION;

    if (ui_auto_is_preset_supported(preset)) {
        const ui_auto_rule_info_t *rule = &s_rules[preset];
        cfg->auto_on_thresh = rule->def_on;
        cfg->auto_off_thresh = rule->def_off;
    } else {
        cfg->auto_on_thresh = 0;
        cfg->auto_off_thresh = 0;
    }
    (void)idx;
}

bool ui_auto_migrate_legacy_config(const void *src, size_t src_sz, ui_hub_device_config_t *dst)
{
    if (src == NULL || dst == NULL || src_sz < sizeof(ui_hub_device_config_v1_t)) {
        return false;
    }
    const ui_hub_device_config_v1_t *legacy = (const ui_hub_device_config_v1_t *)src;
    uint8_t preset = legacy->preset;
    if (preset >= UI_PRESET_COUNT) preset = UI_PRESET_GENERIC;

    memset(dst, 0, sizeof(*dst));
    dst->preset = preset;
    dst->schema_version = UI_CONFIG_SCHEMA_VERSION;
    dst->active_low = (legacy->active_low != 0) ? 1u : 0u;

    /* Copy name safely ensuring bounded null-termination */
    size_t len = 0;
    while (len < sizeof(legacy->name) && legacy->name[len] != '\0') len++;
    if (len == 0 || len >= sizeof(dst->name)) {
        snprintf(dst->name, sizeof(dst->name), "%s", s_preset_default_names[preset]);
    } else {
        memcpy(dst->name, legacy->name, len);
        dst->name[len] = '\0';
    }

    /* Per R1: Legacy config without thresholds always yields Manual mode */
    dst->mode = UI_MODE_MANUAL;
    if (ui_auto_is_preset_supported(preset)) {
        const ui_auto_rule_info_t *rule = &s_rules[preset];
        dst->auto_on_thresh = rule->def_on;
        dst->auto_off_thresh = rule->def_off;
    } else {
        dst->auto_on_thresh = 0;
        dst->auto_off_thresh = 0;
    }
    return true;
}

void ui_auto_state_init(ui_auto_state_t *state, uint32_t now)
{
    if (state == NULL) return;
    memset(state, 0, sizeof(*state));
    (void)now;
}

void ui_auto_restart_hold(ui_auto_state_t *state, uint32_t now)
{
    if (state == NULL) return;
    state->last_transition_ms = (now > 0) ? now : 1u;
    state->qualify_count = 0;
    state->in_qualification = false;
}

void ui_auto_rearm(ui_auto_state_t *state, uint32_t now)
{
    if (state == NULL) return;
    state->paused_error = false;
    state->qualify_count = 0;
    state->in_qualification = false;
    state->last_transition_ms = now;
}

void ui_auto_pause(ui_auto_state_t *state)
{
    if (state == NULL) return;
    state->paused_error = true;
    state->qualify_count = 0;
    state->in_qualification = false;
}

void ui_auto_reset_qualification(ui_auto_state_t *state)
{
    if (state == NULL) return;
    state->qualify_count = 0;
    state->in_qualification = false;
}

bool ui_auto_eval(ui_auto_state_t *state,
                  const ui_hub_device_config_t *cfg,
                  const lora_node_status_t *status,
                  bool link_online,
                  bool current_reported_on,
                  uint32_t now,
                  bool *desired_on_out)
{
    if (state == NULL || cfg == NULL || status == NULL || desired_on_out == NULL) {
        return false;
    }

    /* Auto rule requires UI_MODE_AUTO on a supported preset */
    if (cfg->mode != UI_MODE_AUTO || !ui_auto_is_preset_supported(cfg->preset)) {
        state->qualify_count = 0;
        state->in_qualification = false;
        return false;
    }

    /* If paused on timeout error, do not evaluate until re-armed */
    if (state->paused_error) {
        return false;
    }

    /* Link must be online */
    if (!link_online) {
        state->qualify_count = 0;
        state->in_qualification = false;
        return false;
    }

    /* Input sensor validity check and relevant fault masking (per R5) */
    int32_t val = 0;
    bool target_on = false;
    bool sensor_invalid = false;

    if (cfg->preset == UI_PRESET_FAN) {
        /* Relevant faults: rs485_fault or co2_timeout */
        if (!status->bits.co2_valid || status->bits.rs485_fault || status->bits.co2_timeout) {
            sensor_invalid = true;
        } else {
            val = (int32_t)status->bits.co2_ppm;
            if (val > 10000) sensor_invalid = true;
        }
    } else if (cfg->preset == UI_PRESET_PURIFIER) {
        /* Relevant faults: rs485_fault or co2_timeout (VOC sensor shared bus/driver) */
        if (!status->bits.voc_valid || status->bits.rs485_fault || status->bits.co2_timeout) {
            sensor_invalid = true;
        } else {
            val = (int32_t)status->bits.voc_index;
            if (val > 500) sensor_invalid = true;
        }
    } else if (cfg->preset == UI_PRESET_HUMIDIFIER) {
        /* Relevant faults: rs485_fault or th_timeout */
        if (!status->bits.humid_valid || status->bits.rs485_fault || status->bits.th_timeout) {
            sensor_invalid = true;
        } else {
            val = (int32_t)status->bits.humidity_pct;
            if (val > 100) sensor_invalid = true;
        }
    } else {
        sensor_invalid = true;
    }

    if (sensor_invalid) {
        state->suspended_sensor = true;
        state->qualify_count = 0;
        state->in_qualification = false;
        return false;
    }

    /* Sensor is valid. On recovery from suspension, restart 10-second hold once */
    if (state->suspended_sensor) {
        state->suspended_sensor = false;
        state->last_transition_ms = (now > 0) ? now : 1u;
        state->qualify_count = 0;
        state->in_qualification = false;
        return false;
    }

    if (cfg->preset == UI_PRESET_FAN) {
        if (val >= cfg->auto_on_thresh) {
            target_on = true;
        } else if (val <= cfg->auto_off_thresh) {
            target_on = false;
        } else {
            /* Deadband */
            state->qualify_count = 0;
            state->in_qualification = false;
            return false;
        }
    } else if (cfg->preset == UI_PRESET_PURIFIER) {
        if (val >= cfg->auto_on_thresh) {
            target_on = true;
        } else if (val <= cfg->auto_off_thresh) {
            target_on = false;
        } else {
            /* Deadband */
            state->qualify_count = 0;
            state->in_qualification = false;
            return false;
        }
    } else if (cfg->preset == UI_PRESET_HUMIDIFIER) {
        /* Humidifier turns ON when dry (<= on_thresh), OFF when humid (>= off_thresh) */
        if (val <= cfg->auto_on_thresh) {
            target_on = true;
        } else if (val >= cfg->auto_off_thresh) {
            target_on = false;
        } else {
            /* Deadband */
            state->qualify_count = 0;
            state->in_qualification = false;
            return false;
        }
    }

    /* If target equals current reported state, settled; reset qualification */
    if (target_on == current_reported_on) {
        state->qualify_count = 0;
        state->in_qualification = false;
        return false;
    }

    /* If 10-second post-switch hold is active, ignore opposite-direction rule triggers (per B5) */
    if (state->last_transition_ms != 0 && (now - state->last_transition_ms) < UI_AUTO_MIN_HOLD_MS) {
        state->qualify_count = 0;
        state->in_qualification = false;
        return false;
    }

    /* Accumulate qualification frames */
    if (!state->in_qualification || state->qualify_target_on != target_on) {
        state->in_qualification = true;
        state->qualify_target_on = target_on;
        state->qualify_count = 1u;
    } else {
        if (state->qualify_count < UI_AUTO_QUALIFY_FRAMES) {
            state->qualify_count++;
        }
    }

    /* Check 3-frame qualification requirement */
    if (state->qualify_count < UI_AUTO_QUALIFY_FRAMES) {
        return false;
    }

    /* Qualified and 10-second hold satisfied: trigger command */
    state->qualify_count = 0;
    state->in_qualification = false;
    *desired_on_out = target_on;
    return true;
}

void ui_auto_step_threshold(uint8_t preset, int32_t *thresh, int32_t dir)
{
    if (thresh == NULL || !ui_auto_is_preset_supported(preset)) return;
    const ui_auto_rule_info_t *rule = ui_auto_get_rule_info(preset);
    if (rule == NULL) return;
    int32_t step = rule->step;
    int32_t val = *thresh + (dir > 0 ? step : -step);
    if (val < rule->min_val) val = rule->min_val;
    if (val > rule->max_val) val = rule->max_val;
    *thresh = val;
}
