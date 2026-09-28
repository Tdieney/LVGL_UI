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
    uint32_t rx_elapsed = (now == 0 && lora_last_rx_tick_ms == 1u) ? 0 : (now - lora_last_rx_tick_ms);
    bool online = (lora_last_rx_tick_ms != 0) && (rx_elapsed < LORA_LINK_TIMEOUT_MS);
    s_view_snapshot.connection = online ? UI_CONN_CONNECTED : UI_CONN_DISCONNECTED;

    /* Metrics from lora_node_status */
    s_view_snapshot.metrics[UI_METRIC_CO2].value = lora_node_status.bits.co2_ppm;
    s_view_snapshot.metrics[UI_METRIC_CO2].valid = online && lora_node_status.bits.co2_valid;

    s_view_snapshot.metrics[UI_METRIC_VOC].value = lora_node_status.bits.tvoc_ppb;
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

const char *ui_get_trends_title_str(void)
{
    const ui_test_hooks_t *h = ui_get_test_hooks();
    if (h && h->trends_title_lbl && *(h->trends_title_lbl)) {
        return lv_label_get_text(*(h->trends_title_lbl));
    }
    return NULL;
}

const char *ui_get_trends_unit_str(void)
{
    const ui_test_hooks_t *h = ui_get_test_hooks();
    if (h && h->trends_unit_lbl && *(h->trends_unit_lbl)) {
        return lv_label_get_text(*(h->trends_unit_lbl));
    }
    return NULL;
}

bool ui_is_trends_unavail_badge_hidden(void)
{
    const ui_test_hooks_t *h = ui_get_test_hooks();
    if (h && h->trends_unavail_badge && *(h->trends_unavail_badge)) {
        return lv_obj_has_flag(*(h->trends_unavail_badge), LV_OBJ_FLAG_HIDDEN);
    }
    return true;
}

bool ui_is_trends_empty_box_hidden(void)
{
    const ui_test_hooks_t *h = ui_get_test_hooks();
    if (h && h->trends_empty_box && *(h->trends_empty_box)) {
        return lv_obj_has_flag(*(h->trends_empty_box), LV_OBJ_FLAG_HIDDEN);
    }
    return true;
}

const char *ui_get_trends_empty_lbl_str(void)
{
    const ui_test_hooks_t *h = ui_get_test_hooks();
    if (h && h->trends_empty_lbl && *(h->trends_empty_lbl)) {
        return lv_label_get_text(*(h->trends_empty_lbl));
    }
    return NULL;
}

bool ui_is_trends_empty_hint_hidden(void)
{
    const ui_test_hooks_t *h = ui_get_test_hooks();
    if (h && h->trends_empty_hint && *(h->trends_empty_hint)) {
        return lv_obj_has_flag(*(h->trends_empty_hint), LV_OBJ_FLAG_HIDDEN);
    }
    return true;
}

const char *ui_get_trends_y_lbl_str(uint8_t idx)
{
    const ui_test_hooks_t *h = ui_get_test_hooks();
    if (h && h->trends_y_lbls && idx < 3 && h->trends_y_lbls[idx] != NULL &&
        !lv_obj_has_flag(h->trends_y_lbls[idx], LV_OBJ_FLAG_HIDDEN)) {
        return h->trends_y_buf ? h->trends_y_buf[idx] : lv_label_get_text(h->trends_y_lbls[idx]);
    }
    return NULL;
}

const char *ui_get_trends_time_lbl_str(uint8_t idx)
{
    const ui_test_hooks_t *h = ui_get_test_hooks();
    if (h && h->trends_time_labels && idx < 4 && h->trends_time_labels[idx] != NULL &&
        !lv_obj_has_flag(h->trends_time_labels[idx], LV_OBJ_FLAG_HIDDEN)) {
        return h->time_axis_buf ? h->time_axis_buf[idx] : lv_label_get_text(h->trends_time_labels[idx]);
    }
    return NULL;
}

bool ui_is_trends_tab_checked(uint8_t idx)
{
    const ui_test_hooks_t *h = ui_get_test_hooks();
    if (h && h->trends_tab_cards && idx < 4 && h->trends_tab_cards[idx] != NULL) {
        return lv_obj_has_state(h->trends_tab_cards[idx], LV_STATE_CHECKED);
    }
    return false;
}

bool ui_is_trends_tab_dot_hidden(uint8_t idx)
{
    const ui_test_hooks_t *h = ui_get_test_hooks();
    if (h && h->trends_tab_dots && idx < 4 && h->trends_tab_dots[idx] != NULL) {
        return lv_obj_has_flag(h->trends_tab_dots[idx], LV_OBJ_FLAG_HIDDEN);
    }
    return true;
}

const char *ui_get_trends_tab_val_str(uint8_t idx)
{
    const ui_test_hooks_t *h = ui_get_test_hooks();
    if (h && h->trends_tab_vals && idx < 4 && h->trends_tab_vals[idx] != NULL) {
        return h->trends_tab_val_buf ? h->trends_tab_val_buf[idx] : lv_label_get_text(h->trends_tab_vals[idx]);
    }
    return NULL;
}

const char *ui_get_trends_tab_unit_str(uint8_t idx)
{
    const ui_test_hooks_t *h = ui_get_test_hooks();
    if (h && h->trends_tab_units && idx < 4 && h->trends_tab_units[idx] != NULL) {
        return h->trends_tab_unit_buf ? h->trends_tab_unit_buf[idx] : lv_label_get_text(h->trends_tab_units[idx]);
    }
    return NULL;
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
        if (h->auto_states) {
            ui_auto_state_t *ast = &((ui_auto_state_t *)h->auto_states)[i];
            ast->paused_error = false;
            ast->qualify_count = 0;
            ast->in_qualification = false;
            ast->last_transition_ms = 0;
            ast->suspended_sensor = false;
        }
    }
    ui_dismiss_toast();
}

void ui_test_reset_history(void)
{
    const ui_test_hooks_t *h = ui_get_test_hooks();
    if (h && h->reset_history) {
        h->reset_history();
    }
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

bool ui_is_auto_hold_active(uint8_t idx)
{
    const ui_test_hooks_t *h = ui_get_test_hooks();
    if (!h || !h->auto_states || idx >= UI_DEVICE_COUNT) return false;
    uint32_t last = h->auto_states[idx].last_transition_ms;
    if (last == 0) return false;
    uint32_t now = lv_tick_get();
    uint32_t elapsed = (now == 0 && last == 1u) ? 0 : (now - last);
    return (elapsed < UI_AUTO_MIN_HOLD_MS);
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

static sim_tx_record_t s_sim_tx_records[SIM_MAX_TX_RECORDS];
static size_t s_sim_tx_count = 0;
static void (*s_tx_sink)(uint32_t now, const lora_hub_cmd_t *cmd) = NULL;

void sim_set_tx_sink(void (*sink)(uint32_t now, const lora_hub_cmd_t *cmd))
{
    s_tx_sink = sink;
}

void sim_reset_tx_records(void)
{
    s_sim_tx_count = 0;
}

void sim_record_tx(uint32_t now, const lora_hub_cmd_t *cmd)
{
    if (s_sim_tx_count < SIM_MAX_TX_RECORDS) {
        sim_tx_record_t *r = &s_sim_tx_records[s_sim_tx_count++];
        r->tick_ms = now;
        memcpy(r->bytes, cmd->bytes, sizeof(lora_hub_cmd_t));
        r->seq = cmd->bits.seq;
        r->relay_mask = lora_hub_cmd_get_relay_gpios_mask(cmd);
        r->tx_ready_at_send = ui_is_tx_ready();
    }
    if (s_tx_sink != NULL) {
        s_tx_sink(now, cmd);
    }
}

size_t sim_get_tx_count(void)
{
    return s_sim_tx_count;
}

const sim_tx_record_t *sim_get_tx_record(size_t index)
{
    return (index < s_sim_tx_count) ? &s_sim_tx_records[index] : NULL;
}

const sim_tx_record_t *sim_get_tx_records(void)
{
    return s_sim_tx_records;
}

