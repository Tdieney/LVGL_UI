#ifndef UI_TYPES_H
#define UI_TYPES_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define UI_DEVICE_COUNT 4u
#define UI_HISTORY_CAPACITY 16u

typedef enum {
    UI_CONN_DISCONNECTED = 0,
    UI_CONN_CONNECTED = 1
} ui_connection_state_t;

typedef enum {
    UI_QUALITY_UNKNOWN = 0,
    UI_QUALITY_GOOD,
    UI_QUALITY_MODERATE,
    UI_QUALITY_POOR
} ui_quality_category_t;

typedef enum {
    UI_METRIC_CO2 = 0,
    UI_METRIC_VOC = 1,
    UI_METRIC_TEMP = 2,
    UI_METRIC_HUMIDITY = 3,
    UI_METRIC_COUNT = 4
} ui_metric_id_t;

typedef enum {
    UI_MODE_AUTO = 0,
    UI_MODE_MANUAL = 1
} ui_device_mode_t;

typedef enum {
    UI_PRESET_PURIFIER = 0,
    UI_PRESET_FAN = 1,
    UI_PRESET_HUMIDIFIER = 2,
    UI_PRESET_DEHUMIDIFIER = 3,
    UI_PRESET_HEATER = 4,
    UI_PRESET_LIGHT = 5,
    UI_PRESET_PLUG = 6,
    UI_PRESET_GENERIC = 7,
    UI_PRESET_COUNT = 8
} ui_preset_id_t;

typedef struct {
    const char *name;
    const char *icon_name;
} ui_preset_desc_t;

typedef struct {
    int32_t value;
    bool valid;
    uint16_t minute_of_day;         /* e.g. 14*60 + 17 = 857 */
} ui_history_sample_t;

#define UI_CONFIG_SCHEMA_VERSION 0x02u

/* Legacy v1 config struct (for Flash migration and regression testing) */
typedef struct {
    char name[24];
    uint8_t preset;                 /* 0..7 */
    ui_device_mode_t mode;          /* Auto or Manual */
    uint8_t active_low;             /* 0 = Active High (GPIO 1 is ON), 1 = Active Low (GPIO 0 is ON) */
} ui_hub_device_config_v1_t;

/* Hub-local device configuration (stored locally on Hub, never transmitted on wire) */
typedef struct {
    char name[24];
    uint8_t preset;                 /* 0..7 */
    ui_device_mode_t mode;          /* Auto or Manual */
    uint8_t active_low;             /* 0 = Active High (GPIO 1 is ON), 1 = Active Low (GPIO 0 is ON) */
    uint8_t schema_version;         /* UI_CONFIG_SCHEMA_VERSION */
    int32_t auto_on_thresh;         /* Threshold to turn ON (CO2: ppm, VOC: index, RH: %) */
    int32_t auto_off_thresh;        /* Threshold to turn OFF */
} ui_hub_device_config_t;

/* Optional configuration change notification hook (for Flash persistence, etc.) */
typedef void (*ui_config_changed_cb_t)(uint8_t device_idx, const ui_hub_device_config_t *config);

#ifdef __cplusplus
}
#endif

#endif /* UI_TYPES_H */
