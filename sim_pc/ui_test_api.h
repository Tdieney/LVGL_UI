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

#define UI_TRENDS_SETTLE_MS 150u
#ifndef UI_CMD_TIMEOUT_MS
#define UI_CMD_TIMEOUT_MS 3000u
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
/* Trends page testing and inspection helpers */
ui_metric_id_t ui_get_selected_trend_metric(void);
const ui_history_sample_t *ui_get_history_sample(ui_metric_id_t metric, uint16_t index);
void ui_test_reset_history(void);
const char *ui_get_trends_title_str(void);
const char *ui_get_trends_unit_str(void);
bool ui_is_trends_unavail_badge_hidden(void);
bool ui_is_trends_empty_box_hidden(void);
const char *ui_get_trends_empty_lbl_str(void);
bool ui_is_trends_empty_hint_hidden(void);
const char *ui_get_trends_y_lbl_str(uint8_t idx);
const char *ui_get_trends_time_lbl_str(uint8_t idx);
bool ui_is_trends_tab_checked(uint8_t idx);
bool ui_is_trends_tab_dot_hidden(uint8_t idx);
const char *ui_get_trends_tab_val_str(uint8_t idx);
const char *ui_get_trends_tab_unit_str(uint8_t idx);

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
bool ui_is_auto_hold_active(uint8_t idx);
bool ui_is_tx_ready_test(void);
bool test_restore_pre_init_fixture(void);
bool test_restore_post_init_assertion(void);
bool test_restore_integration_suite(void);
void ui_test_set_boot_staging(bool active);
void ui_test_reset_device_configs(void);
void ui_dialog_open_dropdown(void);
void ui_dialog_close_dropdown(void);
struct _lv_obj_t *ui_dialog_get_dropdown_list(void);
bool ui_test_is_link_connected(uint32_t now);

/* Host TX Recording Structures & Integration Loop Stub (W3/X3/Y3) */
typedef struct {
    uint32_t tick_ms;
    uint8_t bytes[sizeof(lora_hub_cmd_t)];
    uint16_t seq;
    uint8_t relay_mask;
    bool tx_ready_at_send;
} sim_tx_record_t;

#define SIM_MAX_TX_RECORDS 128

void sim_set_tx_sink(void (*sink)(uint32_t now, const lora_hub_cmd_t *cmd));
void sim_reset_tx_records(void);
void sim_record_tx(uint32_t now, const lora_hub_cmd_t *cmd);
size_t sim_get_tx_count(void);
const sim_tx_record_t *sim_get_tx_record(size_t index);
const sim_tx_record_t *sim_get_tx_records(void);

#ifdef __cplusplus
}
#endif

#endif /* UI_TEST_API_H */
