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
    const uint32_t *history_rev;
    const bool *history_externally_fed;
    const bool *pending_trend_chart;
    const bool *stage1_flush_pending;
    const ui_metric_id_t *pending_trend_metric;
    void (*trigger_metric_tab_click)(ui_metric_id_t metric);
    lv_obj_t **trends_title_lbl;

    /* Text buffers */
    const char *co2_val_buf;
    const char *voc_val_buf;
    const char *temp_val_buf;
    const char *humid_val_buf;
    const char *co2_badge_buf;
    const char *voc_badge_buf;
    const char (*dev_name_buf)[24];
    const char (*dev_state_buf)[24];
    const char (*dev_mode_buf)[16];
    const char (*trends_y_buf)[16];
    const char (*time_axis_buf)[16];
    const char (*trends_tab_val_buf)[16];
    const char (*trends_tab_unit_buf)[8];

    /* Trends Widgets */
    lv_obj_t **trends_chart;
    lv_chart_series_t **trends_chart_series;
    lv_obj_t **trends_unit_lbl;
    lv_obj_t **trends_unavail_badge;
    lv_obj_t **trends_empty_box;
    lv_obj_t **trends_empty_lbl;
    lv_obj_t **trends_empty_hint;
    lv_obj_t **trends_y_lbls;
    lv_obj_t **trends_tab_cards;
    lv_obj_t **trends_tab_icons;
    lv_obj_t **trends_tab_names;
    lv_obj_t **trends_tab_dots;
    lv_obj_t **trends_tab_vals;
    lv_obj_t **trends_tab_units;
    lv_obj_t **trends_time_labels;

    /* Home & Devices Widgets */
    lv_obj_t **home_co2_val;
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
    lv_obj_t **dialog_btn_on_minus;
    lv_obj_t **dialog_btn_on_plus;
    lv_obj_t **dialog_btn_off_minus;
    lv_obj_t **dialog_btn_off_plus;
    void (*step_threshold)(bool is_on, int32_t dir);
    void (*on_dialog_reset_limits_click)(lv_event_t *e);
    void (*on_dialog_limits_back_click)(lv_event_t *e);
    void (*on_dialog_open_limits_click)(lv_event_t *e);

    /* Header widgets */
    lv_obj_t **link_label;
    lv_obj_t **link_bars_cont;
    lv_obj_t **header_sep;
    lv_obj_t **uptime_label;
    lv_obj_t **header_clock_ico;

    /* Additional test inspection widgets */
    lv_obj_t **home_temp_note;
    lv_obj_t **home_humid_note;
    lv_obj_t **home_air_status;
    lv_obj_t **dialog_live_lbl;
    void (*reset_history)(void);
} ui_test_hooks_t;

const ui_test_hooks_t *ui_get_test_hooks(void);

#ifdef __cplusplus
}
#endif

#endif /* UI_TEST_HOOKS_H */
