#ifndef UI_H
#define UI_H

#include "ui_types.h"
#include "ui_auto.h"
#include "lora_comm.h"
#include "lora_hub_link.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    UI_PAGE_HOME = 0,
    UI_PAGE_TRENDS = 1,
    UI_PAGE_DEVICES = 2
} ui_page_id_t;

/* Initialize UI subsystem and build shell */
void ui_init(void);

/* Bounded periodic update hook (call on UI context, never ISR) */
void ui_tick(void);

/* Outbound transmit readiness gate (true only after first fresh report is reconciled) */
bool ui_is_tx_ready(void);

/* Hub-local device configuration API */
const ui_hub_device_config_t *ui_get_device_config(uint8_t idx);
bool ui_set_device_config(uint8_t idx, const ui_hub_device_config_t *cfg);
void ui_set_config_changed_cb(ui_config_changed_cb_t cb);

/* Ingest trend history points */
void ui_feed_history(ui_metric_id_t metric, const int32_t *values, const bool *valid, uint16_t count, uint16_t start_minute);

/* Navigation API */
void ui_navigate_to_page(ui_page_id_t page);
void ui_select_trend_metric(ui_metric_id_t metric);

/* Replay splash screen */
void ui_replay_splash(void);

/* Transient toast announcement & immediate dismissal */
void ui_announce(const char *message);
void ui_dismiss_toast(void);

/* Get current monotonic uptime string */
const char *ui_get_uptime_str(void);

/* Get current signal indicator bar count (0 = disconnected, 1..4 = bars) */
uint8_t ui_get_link_bars(void);

/* Get current link status label string ("Connected" or "Disconnected") */
const char *ui_get_link_status_str(void);

#ifdef __cplusplus
}
#endif

#endif /* UI_H */
