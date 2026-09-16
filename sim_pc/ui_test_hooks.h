#ifndef UI_TEST_HOOKS_H
#define UI_TEST_HOOKS_H

#include "ui.h"
#include "ui_internal.h"
#include "lora_comm.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const ui_hub_device_config_t *dev_configs;
    const bool *desired_on;
    const ui_device_cmd_state_t *device_cmds;
    const ui_preset_desc_t *presets;
    uint32_t *uptime_seconds;
    void (*update_uptime_string)(void);
    void (*format_temp_c)(char *buf, size_t buf_size, int32_t tenths);
    void (*toggle_device)(uint8_t idx, bool is_retry);
    void (*open_device_dialog)(uint8_t device_idx);
    void (*close_device_dialog)(void);
    void (*on_dialog_save_click)(lv_event_t *e);

    /* History & Metrics */
    const ui_history_sample_t (*history)[UI_HISTORY_CAPACITY];
    const uint16_t *history_count;
    const ui_metric_id_t *selected_metric;

    /* Text buffers */
    const char *stat_cur_buf;
    const char *stat_min_buf;
    const char *stat_max_buf;
    const char *chart_tooltip_buf;
    const char *co2_val_buf;
    const char *voc_val_buf;
    const char *temp_val_buf;
    const char *humid_val_buf;
    const char *co2_badge_buf;
    const char *voc_badge_buf;
    const char (*dev_name_buf)[24];
    const char (*dev_state_buf)[24];
    const char (*dev_mode_buf)[16];

    /* Widgets */
    lv_obj_t **trends_chart;
    lv_chart_series_t **trends_chart_series;
    lv_obj_t **trends_tooltip;
    lv_obj_t **dev_page_names;
    lv_obj_t **dev_page_modes;
    lv_obj_t **dev_page_states;
    lv_obj_t **dev_page_switches;
    lv_obj_t **dev_page_retries;
    lv_obj_t **dev_page_dashes;
    lv_obj_t **home_dev_names;
    lv_obj_t **home_dev_modes;
    lv_obj_t **home_dev_states;
    lv_obj_t **home_dev_switches;
    lv_obj_t **home_dev_retries;
    lv_obj_t **home_dev_dashes;
    lv_obj_t **dialog_overlay;
    lv_obj_t **dialog_box;
    lv_obj_t **dialog_btn_x;
    lv_obj_t **dialog_preset_dd;
    uint8_t *dialog_edit_preset;
    ui_device_mode_t *dialog_edit_mode;
    uint8_t *dialog_edit_active_low;
    lv_obj_t **dialog_mode_btns;
    lv_obj_t **dialog_mode_labels;
    const ui_device_mode_t *dialog_btn_modes;
    lv_obj_t **dialog_active_low_sw;

    /* Auto v1 & Threshold Editor hooks */
    const ui_auto_state_t *auto_states;
    int32_t *dialog_edit_on_thresh;
    int32_t *dialog_edit_off_thresh;
    const char *dialog_validation_buf;
    lv_obj_t **dialog_subview_device;
    lv_obj_t **dialog_subview_auto;
    lv_obj_t **dialog_btn_limits;
    lv_obj_t **dialog_btn_save_auto;
    void (*step_threshold)(bool is_on, int32_t dir);
    void (*on_dialog_reset_limits_click)(lv_event_t *e);
    void (*on_dialog_limits_back_click)(lv_event_t *e);
    void (*on_dialog_open_limits_click)(lv_event_t *e);

    /* Header widgets */
    lv_obj_t **link_label;
    lv_obj_t **link_bars_cont;
    lv_obj_t **header_sep;
    lv_obj_t **uptime_label;
} ui_test_hooks_t;

const ui_test_hooks_t *ui_get_test_hooks(void);

#ifdef __cplusplus
}
#endif

#endif /* UI_TEST_HOOKS_H */
