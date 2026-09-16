#include "ui_test_api.h"
#include "ui_test_hooks.h"
#include "ui_theme.h"
#include "lora_comm.h"
#include "lora_hub_link.h"
#include <stdio.h>
#include <string.h>

/* Dedicated view snapshot held only in simulator test runner (P2.9) */
static ui_snapshot_t s_view_snapshot;

void ui_open_device_settings(uint8_t device_index)
{
    const ui_test_hooks_t *h = ui_get_test_hooks();
    if (h && h->open_device_dialog) h->open_device_dialog(device_index);
}

void ui_close_device_settings(void)
{
    const ui_test_hooks_t *h = ui_get_test_hooks();
    if (h && h->close_device_dialog) h->close_device_dialog();
}

bool ui_is_device_settings_open(void)
{
    const ui_test_hooks_t *h = ui_get_test_hooks();
    if (h && h->dialog_overlay) return *(h->dialog_overlay) != NULL;
    return false;
}

void ui_save_device_settings(void)
{
    const ui_test_hooks_t *h = ui_get_test_hooks();
    if (h && h->on_dialog_save_click) h->on_dialog_save_click(NULL);
}

void ui_trigger_device_toggle(uint8_t device_index)
{
    const ui_test_hooks_t *h = ui_get_test_hooks();
    if (h && h->toggle_device) h->toggle_device(device_index, false);
}

void ui_trigger_device_retry(uint8_t device_index)
{
    const ui_test_hooks_t *h = ui_get_test_hooks();
    if (h && h->toggle_device) h->toggle_device(device_index, true);
}

const ui_snapshot_t *ui_get_snapshot(void)
{
    const ui_test_hooks_t *h = ui_get_test_hooks();
    if (!h || !h->dev_configs || !h->device_cmds) return NULL;

    uint32_t now = lv_tick_get();
    bool online = (lora_last_rx_tick_ms != 0) && ((now - lora_last_rx_tick_ms) < LORA_LINK_TIMEOUT_MS);
    s_view_snapshot.connection = online ? UI_CONN_CONNECTED : UI_CONN_DISCONNECTED;

    /* Metrics from lora_node_status */
    s_view_snapshot.metrics[UI_METRIC_CO2].value = lora_node_status.bits.co2_ppm;
    s_view_snapshot.metrics[UI_METRIC_CO2].valid = online && lora_node_status.bits.co2_valid;

    s_view_snapshot.metrics[UI_METRIC_VOC].value = lora_node_status.bits.voc_index;
    s_view_snapshot.metrics[UI_METRIC_VOC].valid = online && lora_node_status.bits.voc_valid;

    s_view_snapshot.metrics[UI_METRIC_TEMP].value = lora_node_status.bits.temp_deci_c;
    s_view_snapshot.metrics[UI_METRIC_TEMP].valid = online && lora_node_status.bits.temp_valid;

    s_view_snapshot.metrics[UI_METRIC_HUMIDITY].value = lora_node_status.bits.humidity_pct;
    s_view_snapshot.metrics[UI_METRIC_HUMIDITY].valid = online && lora_node_status.bits.humid_valid;

    for (uint8_t i = 0; i < UI_DEVICE_COUNT; i++) {
        const ui_hub_device_config_t *cfg = &h->dev_configs[i];
        const ui_device_cmd_state_t *cmd = &h->device_cmds[i];
        uint8_t reported_gpio = lora_node_status_get_relay_gpio(&lora_node_status, i);
        bool reported_on = (reported_gpio ^ cfg->active_low) != 0;

        snprintf(s_view_snapshot.devices[i].name, sizeof(s_view_snapshot.devices[i].name), "%s", cfg->name);
        s_view_snapshot.devices[i].preset = cfg->preset;
        s_view_snapshot.devices[i].mode = cfg->mode;
        s_view_snapshot.devices[i].valid = online;

        if (cmd->phase == UI_CMD_PHASE_PENDING) {
            s_view_snapshot.devices[i].pending = true;
            s_view_snapshot.devices[i].error = false;
            s_view_snapshot.devices[i].on = h->desired_on ? h->desired_on[i] : reported_on;
        } else if (cmd->phase == UI_CMD_PHASE_ERROR) {
            s_view_snapshot.devices[i].pending = false;
            s_view_snapshot.devices[i].error = true;
            s_view_snapshot.devices[i].on = reported_on;
        } else {
            s_view_snapshot.devices[i].pending = false;
            s_view_snapshot.devices[i].error = false;
            s_view_snapshot.devices[i].on = reported_on;
        }
    }
    return &s_view_snapshot;
}

void ui_set_uptime_seconds(uint32_t seconds)
{
    const ui_test_hooks_t *h = ui_get_test_hooks();
    if (h && h->uptime_seconds) {
        *(h->uptime_seconds) = seconds;
        if (h->update_uptime_string) h->update_uptime_string();
    }
}

void ui_format_temp(char *buf, size_t buf_size, int32_t tenths)
{
    const ui_test_hooks_t *h = ui_get_test_hooks();
    if (h && h->format_temp_c) h->format_temp_c(buf, buf_size, tenths);
}

void ui_inspect_chart_point(uint32_t point_id)
{
    const ui_test_hooks_t *h = ui_get_test_hooks();
    if (h && h->trends_chart && *(h->trends_chart) != NULL && point_id < UI_HISTORY_CAPACITY) {
        lv_obj_t *chart = *(h->trends_chart);
        lv_chart_t *cp = (lv_chart_t *)chart;
        cp->pressed_point_id = (lv_coord_t)point_id;
        lv_event_send(chart, LV_EVENT_VALUE_CHANGED, NULL);
    }
}

const char *ui_get_chart_tooltip_text(void)
{
    const ui_test_hooks_t *h = ui_get_test_hooks();
    if (h && h->trends_tooltip && *(h->trends_tooltip) != NULL &&
        !lv_obj_has_flag(*(h->trends_tooltip), LV_OBJ_FLAG_HIDDEN)) {
        return h->chart_tooltip_buf;
    }
    return NULL;
}

const ui_history_sample_t *ui_get_history_sample(ui_metric_id_t metric, uint16_t index)
{
    const ui_test_hooks_t *h = ui_get_test_hooks();
    if (h && h->history && h->history_count && metric < UI_METRIC_COUNT && index < h->history_count[metric]) {
        return &(h->history[metric][index]);
    }
    return NULL;
}

ui_metric_id_t ui_get_selected_trend_metric(void)
{
    const ui_test_hooks_t *h = ui_get_test_hooks();
    if (h && h->selected_metric) return *(h->selected_metric);
    return UI_METRIC_CO2;
}

const char *ui_get_trends_stat_cur(void)
{
    const ui_test_hooks_t *h = ui_get_test_hooks();
    return h ? h->stat_cur_buf : NULL;
}

const char *ui_get_trends_stat_min(void)
{
    const ui_test_hooks_t *h = ui_get_test_hooks();
    return h ? h->stat_min_buf : NULL;
}

const char *ui_get_trends_stat_max(void)
{
    const ui_test_hooks_t *h = ui_get_test_hooks();
    return h ? h->stat_max_buf : NULL;
}

const char *ui_get_home_co2_str(void)
{
    const ui_test_hooks_t *h = ui_get_test_hooks();
    return h ? h->co2_val_buf : NULL;
}

const char *ui_get_home_voc_str(void)
{
    const ui_test_hooks_t *h = ui_get_test_hooks();
    return h ? h->voc_val_buf : NULL;
}

const char *ui_get_home_temp_str(void)
{
    const ui_test_hooks_t *h = ui_get_test_hooks();
    return h ? h->temp_val_buf : NULL;
}

const char *ui_get_home_humid_str(void)
{
    const ui_test_hooks_t *h = ui_get_test_hooks();
    return h ? h->humid_val_buf : NULL;
}

const char *ui_get_home_co2_badge(void)
{
    const ui_test_hooks_t *h = ui_get_test_hooks();
    return h ? h->co2_badge_buf : NULL;
}

const char *ui_get_home_voc_badge(void)
{
    const ui_test_hooks_t *h = ui_get_test_hooks();
    return h ? h->voc_badge_buf : NULL;
}

const char *ui_get_home_dev_name(uint8_t idx)
{
    const ui_test_hooks_t *h = ui_get_test_hooks();
    if (h && h->dev_name_buf && idx < UI_DEVICE_COUNT) return h->dev_name_buf[idx];
    return NULL;
}

const char *ui_get_home_dev_state(uint8_t idx)
{
    const ui_test_hooks_t *h = ui_get_test_hooks();
    if (h && h->dev_state_buf && idx < UI_DEVICE_COUNT) return h->dev_state_buf[idx];
    return NULL;
}

const char *ui_get_home_dev_mode(uint8_t idx)
{
    const ui_test_hooks_t *h = ui_get_test_hooks();
    if (h && h->dev_mode_buf && idx < UI_DEVICE_COUNT) return h->dev_mode_buf[idx];
    return NULL;
}

const char *ui_get_devices_dev_name(uint8_t idx)
{
    const ui_test_hooks_t *h = ui_get_test_hooks();
    if (h && h->dev_page_names && h->dev_name_buf && idx < UI_DEVICE_COUNT && h->dev_page_names[idx] != NULL) {
        return h->dev_name_buf[idx];
    }
    return NULL;
}

const char *ui_get_devices_dev_mode(uint8_t idx)
{
    const ui_test_hooks_t *h = ui_get_test_hooks();
    if (h && h->dev_page_modes && h->dev_mode_buf && idx < UI_DEVICE_COUNT && h->dev_page_modes[idx] != NULL) {
        return h->dev_mode_buf[idx];
    }
    return NULL;
}

const char *ui_get_devices_dev_state(uint8_t idx)
{
    const ui_test_hooks_t *h = ui_get_test_hooks();
    if (h && h->dev_page_states && h->dev_state_buf && idx < UI_DEVICE_COUNT &&
        h->dev_page_states[idx] != NULL && !lv_obj_has_flag(h->dev_page_states[idx], LV_OBJ_FLAG_HIDDEN)) {
        return h->dev_state_buf[idx];
    }
    return NULL;
}

bool ui_get_devices_switch_checked(uint8_t idx)
{
    const ui_test_hooks_t *h = ui_get_test_hooks();
    if (h && h->dev_page_switches && idx < UI_DEVICE_COUNT && h->dev_page_switches[idx] != NULL) {
        return lv_obj_has_state(h->dev_page_switches[idx], LV_STATE_CHECKED);
    }
    return false;
}

bool ui_get_devices_switch_disabled(uint8_t idx)
{
    const ui_test_hooks_t *h = ui_get_test_hooks();
    if (h && h->dev_page_switches && idx < UI_DEVICE_COUNT && h->dev_page_switches[idx] != NULL) {
        return lv_obj_has_state(h->dev_page_switches[idx], LV_STATE_DISABLED);
    }
    return false;
}

bool ui_get_devices_switch_hidden(uint8_t idx)
{
    const ui_test_hooks_t *h = ui_get_test_hooks();
    if (h && h->dev_page_switches && idx < UI_DEVICE_COUNT && h->dev_page_switches[idx] != NULL) {
        return lv_obj_has_flag(h->dev_page_switches[idx], LV_OBJ_FLAG_HIDDEN);
    }
    return true;
}

bool ui_get_devices_retry_hidden(uint8_t idx)
{
    const ui_test_hooks_t *h = ui_get_test_hooks();
    if (h && h->dev_page_retries && idx < UI_DEVICE_COUNT && h->dev_page_retries[idx] != NULL) {
        return lv_obj_has_flag(h->dev_page_retries[idx], LV_OBJ_FLAG_HIDDEN);
    }
    return true;
}

bool ui_get_devices_dash_hidden(uint8_t idx)
{
    const ui_test_hooks_t *h = ui_get_test_hooks();
    if (h && h->dev_page_dashes && idx < UI_DEVICE_COUNT && h->dev_page_dashes[idx] != NULL) {
        return lv_obj_has_flag(h->dev_page_dashes[idx], LV_OBJ_FLAG_HIDDEN);
    }
    return true;
}

uint8_t ui_get_dialog_edit_preset(void)
{
    const ui_test_hooks_t *h = ui_get_test_hooks();
    if (h && h->dialog_preset_dd && *(h->dialog_preset_dd) != NULL) {
        return (uint8_t)lv_dropdown_get_selected(*(h->dialog_preset_dd));
    }
    return (h && h->dialog_edit_preset) ? *(h->dialog_edit_preset) : 0;
}

ui_device_mode_t ui_get_dialog_edit_mode(void)
{
    const ui_test_hooks_t *h = ui_get_test_hooks();
    return (h && h->dialog_edit_mode) ? *(h->dialog_edit_mode) : UI_MODE_MANUAL;
}

void ui_set_dialog_preset(uint8_t preset)
{
    if (preset >= 8) return;
    const ui_test_hooks_t *h = ui_get_test_hooks();
    if (h && h->dialog_edit_preset) {
        *(h->dialog_edit_preset) = preset;
        if (h->dialog_preset_dd && *(h->dialog_preset_dd) != NULL) {
            lv_dropdown_set_selected(*(h->dialog_preset_dd), preset);
        }
    }
}

void ui_set_dialog_mode(ui_device_mode_t mode)
{
    const ui_test_hooks_t *h = ui_get_test_hooks();
    if (!h || !h->dialog_edit_mode) return;
    *(h->dialog_edit_mode) = mode;
    for (int i = 0; i < 2; i++) {
        if (!h->dialog_mode_btns || h->dialog_mode_btns[i] == NULL) continue;
        if (h->dialog_btn_modes[i] == *(h->dialog_edit_mode)) {
            lv_obj_set_style_bg_color(h->dialog_mode_btns[i], UI_COLOR_BLUE_SOFT, 0);
            if (h->dialog_mode_labels && h->dialog_mode_labels[i]) {
                lv_obj_set_style_text_color(h->dialog_mode_labels[i], UI_COLOR_BLUE_TEXT, 0);
            }
        } else {
            lv_obj_set_style_bg_color(h->dialog_mode_btns[i], lv_color_hex(0xF7F7F7), 0);
            if (h->dialog_mode_labels && h->dialog_mode_labels[i]) {
                lv_obj_set_style_text_color(h->dialog_mode_labels[i], UI_COLOR_INK, 0);
            }
        }
    }
}

uint8_t ui_get_dialog_edit_active_low(void)
{
    const ui_test_hooks_t *h = ui_get_test_hooks();
    if (h && h->dialog_active_low_sw && *(h->dialog_active_low_sw) != NULL) {
        return lv_obj_has_state(*(h->dialog_active_low_sw), LV_STATE_CHECKED) ? 1u : 0u;
    }
    return (h && h->dialog_edit_active_low) ? *(h->dialog_edit_active_low) : 0u;
}

void ui_set_dialog_active_low(uint8_t active_low)
{
    const ui_test_hooks_t *h = ui_get_test_hooks();
    if (h && h->dialog_edit_active_low) {
        *(h->dialog_edit_active_low) = active_low;
        if (h->dialog_active_low_sw && *(h->dialog_active_low_sw) != NULL) {
            if (active_low) {
                lv_obj_add_state(*(h->dialog_active_low_sw), LV_STATE_CHECKED);
            } else {
                lv_obj_clear_state(*(h->dialog_active_low_sw), LV_STATE_CHECKED);
            }
        }
    }
}

uint8_t ui_get_device_active_low(uint8_t idx)
{
    const ui_test_hooks_t *h = ui_get_test_hooks();
    if (h && h->dev_configs && idx < UI_DEVICE_COUNT) {
        return h->dev_configs[idx].active_low;
    }
    return 0u;
}

void ui_test_reset_device_commands(void)
{
    const ui_test_hooks_t *h = ui_get_test_hooks();
    if (!h) return;
    for (uint8_t i = 0; i < UI_DEVICE_COUNT; i++) {
        ui_device_cmd_state_t *cmd = &((ui_device_cmd_state_t *)h->device_cmds)[i];
        cmd->phase = UI_CMD_PHASE_IDLE;
        cmd->request_tick_ms = 0;
        cmd->seq = 0;
        uint8_t reported_gpio = lora_node_status_get_relay_gpio(&lora_node_status, i);
        bool reported_on = (reported_gpio ^ h->dev_configs[i].active_low) != 0;
        ((bool *)h->desired_on)[i] = reported_on;
        lora_hub_cmd_set_relay_gpio(&lora_hub_cmd, i, reported_gpio);
    }
    ui_dismiss_toast();
}

void ui_dialog_open_limits(void)
{
    const ui_test_hooks_t *h = ui_get_test_hooks();
    if (h && h->on_dialog_open_limits_click) {
        h->on_dialog_open_limits_click(NULL);
    }
}

void ui_dialog_limits_back(void)
{
    const ui_test_hooks_t *h = ui_get_test_hooks();
    if (h && h->on_dialog_limits_back_click) {
        h->on_dialog_limits_back_click(NULL);
    }
}

void ui_dialog_reset_limits(void)
{
    const ui_test_hooks_t *h = ui_get_test_hooks();
    if (h && h->on_dialog_reset_limits_click) {
        h->on_dialog_reset_limits_click(NULL);
    }
}

void ui_dialog_step_thresh(bool is_on, int32_t dir)
{
    const ui_test_hooks_t *h = ui_get_test_hooks();
    if (h && h->step_threshold) {
        h->step_threshold(is_on, dir);
    }
}

int32_t ui_get_dialog_edit_on_thresh(void)
{
    const ui_test_hooks_t *h = ui_get_test_hooks();
    return (h && h->dialog_edit_on_thresh) ? *(h->dialog_edit_on_thresh) : 0;
}

int32_t ui_get_dialog_edit_off_thresh(void)
{
    const ui_test_hooks_t *h = ui_get_test_hooks();
    return (h && h->dialog_edit_off_thresh) ? *(h->dialog_edit_off_thresh) : 0;
}

const char *ui_get_dialog_validation_msg(void)
{
    const ui_test_hooks_t *h = ui_get_test_hooks();
    return (h && h->dialog_validation_buf) ? h->dialog_validation_buf : "";
}

bool ui_is_dialog_limits_subview_active(void)
{
    const ui_test_hooks_t *h = ui_get_test_hooks();
    if (h && h->dialog_subview_auto && *(h->dialog_subview_auto)) {
        return !lv_obj_has_flag(*(h->dialog_subview_auto), LV_OBJ_FLAG_HIDDEN);
    }
    return false;
}

bool ui_get_auto_paused_error(uint8_t idx)
{
    const ui_test_hooks_t *h = ui_get_test_hooks();
    if (h && h->auto_states && idx < UI_DEVICE_COUNT) {
        return h->auto_states[idx].paused_error;
    }
    return false;
}

bool ui_is_tx_ready_test(void)
{
    return ui_is_tx_ready();
}

void ui_dialog_open_dropdown(void)
{
    const ui_test_hooks_t *h = ui_get_test_hooks();
    if (h && h->dialog_preset_dd && *(h->dialog_preset_dd)) {
        lv_dropdown_open(*(h->dialog_preset_dd));
    }
}

void ui_dialog_close_dropdown(void)
{
    const ui_test_hooks_t *h = ui_get_test_hooks();
    if (h && h->dialog_preset_dd && *(h->dialog_preset_dd)) {
        lv_dropdown_close(*(h->dialog_preset_dd));
    }
}

struct _lv_obj_t *ui_dialog_get_dropdown_list(void)
{
    const ui_test_hooks_t *h = ui_get_test_hooks();
    if (h && h->dialog_preset_dd && *(h->dialog_preset_dd)) {
        return lv_dropdown_get_list(*(h->dialog_preset_dd));
    }
    return NULL;
}

