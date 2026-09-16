#ifndef UI_TEST_API_H
#define UI_TEST_API_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "ui.h"
#include "ui_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Legacy synthesized snapshot types for simulator assertions (excluded from MCU export) */
typedef struct {
    int32_t value;                  /* Scaled value: CO2 ppm, VOC index (unitless), Temp 0.1 deg C, Humidity % */
    bool valid;                     /* False if sensor fault / missing reading */
    ui_quality_category_t quality;  /* Good, Moderate, Poor, Unknown */
    const char *note;               /* Static caption e.g. "Fresh air", "Sensor unavailable" */
} ui_metric_snapshot_t;

typedef struct {
    char name[24];
    uint8_t preset;                 /* 0..7 */
    bool on;                        /* Reported relay state */
    bool valid;                     /* Output state known */
    ui_device_mode_t mode;          /* Auto or Manual */
    bool pending;                   /* Command in flight */
    bool error;                     /* Command failed / timeout -> show Retry */
    uint32_t pending_req_id;        /* Request ID being awaited */
} ui_device_snapshot_t;

typedef struct {
    ui_connection_state_t connection;
    ui_metric_snapshot_t metrics[UI_METRIC_COUNT];
    ui_device_snapshot_t devices[UI_DEVICE_COUNT];
} ui_snapshot_t;

/* Settings Dialog programmatic control */
void ui_open_device_settings(uint8_t device_index);
void ui_close_device_settings(void);
bool ui_is_device_settings_open(void);
void ui_save_device_settings(void);

/* Programmatic testing and inspection helpers */
void ui_trigger_device_toggle(uint8_t device_index);
void ui_trigger_device_retry(uint8_t device_index);
const ui_snapshot_t *ui_get_snapshot(void);
void ui_test_reset_device_commands(void);
void ui_set_uptime_seconds(uint32_t seconds);
void ui_format_temp(char *buf, size_t buf_size, int32_t tenths);
void ui_inspect_chart_point(uint32_t point_id);
const char *ui_get_chart_tooltip_text(void);
ui_metric_id_t ui_get_selected_trend_metric(void);
const char *ui_get_trends_stat_cur(void);
const char *ui_get_trends_stat_min(void);
const char *ui_get_trends_stat_max(void);
const ui_history_sample_t *ui_get_history_sample(ui_metric_id_t metric, uint16_t index);

/* Home page testing and inspection helpers */
const char *ui_get_home_co2_str(void);
const char *ui_get_home_voc_str(void);
const char *ui_get_home_temp_str(void);
const char *ui_get_home_humid_str(void);
const char *ui_get_home_co2_badge(void);
const char *ui_get_home_voc_badge(void);
const char *ui_get_home_dev_name(uint8_t idx);
const char *ui_get_home_dev_state(uint8_t idx);
const char *ui_get_home_dev_mode(uint8_t idx);

/* Devices page testing and inspection helpers */
const char *ui_get_devices_dev_name(uint8_t idx);
const char *ui_get_devices_dev_mode(uint8_t idx);
const char *ui_get_devices_dev_state(uint8_t idx);
bool ui_get_devices_switch_checked(uint8_t idx);
bool ui_get_devices_switch_disabled(uint8_t idx);
bool ui_get_devices_switch_hidden(uint8_t idx);
bool ui_get_devices_retry_hidden(uint8_t idx);
bool ui_get_devices_dash_hidden(uint8_t idx);
uint8_t ui_get_dialog_edit_preset(void);
ui_device_mode_t ui_get_dialog_edit_mode(void);
uint8_t ui_get_dialog_edit_active_low(void);
uint8_t ui_get_device_active_low(uint8_t idx);
void ui_set_dialog_preset(uint8_t preset);
void ui_set_dialog_mode(ui_device_mode_t mode);
void ui_set_dialog_active_low(uint8_t active_low);

/* Auto v1 and Threshold Editor testing helpers */
void ui_dialog_open_limits(void);
void ui_dialog_limits_back(void);
void ui_dialog_reset_limits(void);
void ui_dialog_step_thresh(bool is_on, int32_t dir);
int32_t ui_get_dialog_edit_on_thresh(void);
int32_t ui_get_dialog_edit_off_thresh(void);
const char *ui_get_dialog_validation_msg(void);
bool ui_is_dialog_limits_subview_active(void);
bool ui_get_auto_paused_error(uint8_t idx);
bool ui_is_tx_ready_test(void);
bool test_restore_pre_init_fixture(void);
bool test_restore_integration_suite(void);
void ui_test_set_boot_staging(bool active);
void ui_test_reset_device_configs(void);
void ui_dialog_open_dropdown(void);
void ui_dialog_close_dropdown(void);
struct _lv_obj_t *ui_dialog_get_dropdown_list(void);

#ifdef __cplusplus
}
#endif

#endif /* UI_TEST_API_H */
