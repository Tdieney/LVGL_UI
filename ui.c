#include "ui.h"
#include "ui_types.h"
#include "ui_auto.h"
#include "ui_theme.h"
#include "ui_icons.h"
#include "ui_fonts.h"
#include "lora_comm.h"
#include "lora_hub_link.h"

#include <stdio.h>
#include <string.h>

/* =========================================================================
 * Wire & Link Globals (Shared State Contract - motor_comm convention)
 *
 * Defined here in ui.c (the UI component) and declared extern in
 * lora_comm.h / lora_hub_link.h. Any MCU application or simulator linking
 * with smart_hub_ui gets these storage definitions out-of-the-box, matching
 * the motorCmd convention from ui/motor-control.
 * ========================================================================= */
lora_hub_cmd_t     lora_hub_cmd;
lora_node_status_t lora_node_status;
volatile uint32_t  lora_last_rx_tick_ms = 0;
volatile uint32_t  lora_rx_revision = 0;
volatile int8_t    lora_last_snr = 8;
volatile int16_t   lora_last_rssi = -65;

/* =========================================================================
 * Shell and Global Object Pointers
 * ========================================================================= */
static lv_obj_t *s_screen;
static lv_obj_t *s_header;
static lv_obj_t *s_content_area;
static lv_obj_t *s_nav_bar;
static lv_obj_t *s_nav_btns[3];
static lv_obj_t *s_nav_icons[3];

/* Header widgets */
static lv_obj_t *s_link_bars_cont;
static lv_obj_t *s_link_bars[4];
static uint8_t   s_current_link_bars = 0;
static lv_obj_t *s_link_label;

/* Toast widget */
static lv_obj_t *s_toast_obj;
static lv_obj_t *s_toast_label;
static lv_timer_t *s_toast_timer = NULL;

/* Splash widgets */
static lv_obj_t *s_splash_screen = NULL;
static lv_obj_t *s_splash_logo = NULL;
static lv_timer_t *s_splash_timer = NULL;
static uint32_t s_splash_start_tick = 0;
static uint8_t s_splash_phase = 0;

/* Active Page */
static ui_page_id_t s_current_page = UI_PAGE_HOME;
static ui_metric_id_t s_selected_metric = UI_METRIC_CO2;
static bool s_pending_trend_chart = false;
static ui_metric_id_t s_pending_trend_metric = UI_METRIC_CO2;
static bool s_stage1_flush_pending = false;
static uint32_t s_pending_trend_tick = 0;
static lv_obj_t *s_page_container = NULL;

/* Device settings dialog and subviews (B2) */
static lv_obj_t *s_dialog_overlay = NULL;
static int8_t s_dialog_device_idx = -1;
static uint8_t s_dialog_edit_preset = 0;
static ui_device_mode_t s_dialog_edit_mode = UI_MODE_MANUAL;
static uint8_t s_dialog_edit_active_low = 0;
static int32_t s_dialog_edit_on_thresh = 0;
static int32_t s_dialog_edit_off_thresh = 0;

static lv_obj_t *s_dialog_box = NULL;
static lv_obj_t *s_dialog_btn_x = NULL;
static lv_obj_t *s_dialog_subview_device = NULL;
static lv_obj_t *s_dialog_subview_auto = NULL;
static lv_obj_t *s_dialog_btn_reset = NULL;

static lv_obj_t *s_dialog_mode_btns[2] = {NULL, NULL};
static lv_obj_t *s_dialog_mode_labels[2] = {NULL, NULL};
static lv_obj_t *s_dialog_preset_dd = NULL;
static lv_obj_t *s_dialog_dd_scrim = NULL;
static lv_obj_t *s_dialog_active_low_sw = NULL;
static lv_obj_t *s_dialog_active_low_btns[2] = {NULL, NULL};
static lv_obj_t *s_dialog_active_low_labels[2] = {NULL, NULL};
static lv_obj_t *s_dialog_btn_limits = NULL;
static lv_obj_t *s_dialog_limits_summary_lbl = NULL;
static lv_obj_t *s_dialog_limits_chevron = NULL;
static lv_obj_t *s_dialog_mode_helper_lbl = NULL;
static lv_obj_t *s_dialog_device_title_lbl = NULL;
static lv_obj_t *s_dialog_device_icon = NULL;

/* Subview auto threshold widgets */
static lv_obj_t *s_dialog_auto_rule_lbl = NULL;
static lv_obj_t *s_dialog_live_lbl = NULL;
static lv_obj_t *s_dialog_live_unit_lbl = NULL;
static char s_dialog_live_unit_buf[16] = "";
static lv_obj_t *s_dialog_limit_cards[2] = {NULL, NULL};
static char s_dialog_live_buf[32];
static char s_dialog_limits_summary[96];
static lv_obj_t *s_dialog_on_op_lbl = NULL;
static lv_obj_t *s_dialog_off_op_lbl = NULL;
static lv_obj_t *s_dialog_on_val_lbl = NULL;
static lv_obj_t *s_dialog_off_val_lbl = NULL;
static lv_obj_t *s_dialog_on_unit_lbl = NULL;
static lv_obj_t *s_dialog_off_unit_lbl = NULL;
static char s_dialog_auto_rule_buf[64] = "";
static char s_dialog_on_op_buf[96] = "";
static char s_dialog_off_op_buf[96] = "";
static char s_dialog_on_val_buf[16] = "";
static char s_dialog_off_val_buf[16] = "";
static char s_dialog_on_unit_buf[16] = "";
static char s_dialog_off_unit_buf[16] = "";
static lv_obj_t *s_dialog_validation_lbl = NULL;
static char s_dialog_validation_buf[128] = "";
static lv_obj_t *s_dialog_btn_save_auto = NULL;
static lv_obj_t *s_dialog_btn_save_dev = NULL;
static lv_obj_t *s_dialog_btn_on_minus = NULL;
static lv_obj_t *s_dialog_btn_on_plus = NULL;
static lv_obj_t *s_dialog_btn_off_minus = NULL;
static lv_obj_t *s_dialog_btn_off_plus = NULL;
static lv_obj_t *s_dialog_on_val_box = NULL;
static lv_obj_t *s_dialog_off_val_box = NULL;
static lv_obj_t *s_dialog_warn_ico = NULL;
static int8_t s_last_dialog_valid_state = -1;

#include "ui_internal.h"
#if defined(UI_TEST_HOOKS)
#include "ui_test_hooks.h"
#endif

/* Hub-local device configuration and state */
static ui_hub_device_config_t s_dev_configs[UI_DEVICE_COUNT];
static bool s_dev_configured[UI_DEVICE_COUNT] = {false};
static bool s_desired_on[UI_DEVICE_COUNT];
static ui_device_cmd_state_t s_device_cmds[UI_DEVICE_COUNT];
static ui_auto_state_t s_auto_states[UI_DEVICE_COUNT];
static ui_config_changed_cb_t s_config_changed_cb = NULL;

/* Gated outbound transmit readiness (A2) */
static bool s_tx_ready = false;
static uint32_t s_last_node_uptime = 0;
static uint8_t s_last_auto_sample_seq = 0;
static bool s_have_node_sample = false;

/* Revision and link state caching for 0-flush idle polling */
static uint32_t s_last_rendered_rx_revision = 0xFFFFFFFFu;
static bool s_last_rendered_link_state = false;

/* Trends caching for 0-flush idle updates (A4) */
static uint32_t s_history_rendered_rev[UI_METRIC_COUNT] = {0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu};
static uint32_t s_history_rev[UI_METRIC_COUNT] = {1u, 1u, 1u, 1u};
static ui_metric_id_t s_last_rendered_metric = (ui_metric_id_t)0xFF;

static inline bool is_link_connected(uint32_t now)
{
    if (lora_last_rx_tick_ms == 0) return false;
    uint32_t elapsed = (now == 0 && lora_last_rx_tick_ms == 1u) ? 0 : (now - lora_last_rx_tick_ms);
    return (elapsed < LORA_LINK_TIMEOUT_MS);
}

/* History storage for 4 metrics (16 samples each) */
static ui_history_sample_t s_history[UI_METRIC_COUNT][UI_HISTORY_CAPACITY];
static uint16_t s_history_count[UI_METRIC_COUNT];
static bool s_history_externally_fed[UI_METRIC_COUNT];
static bool s_auto_hist_started[UI_METRIC_COUNT];
static uint32_t s_auto_hist_last_minute[UI_METRIC_COUNT];
static lv_coord_t s_chart_ext_y[UI_HISTORY_CAPACITY];

/* Dedicated process-lifetime static text buffers (P1.5) */
static char s_uptime_buf[16] = "00:00:00";
static char s_co2_val_buf[16] = "—";
static char s_co2_badge_buf[16] = "Unknown";
static char s_voc_val_buf[16] = "—";
static char s_voc_badge_buf[16] = "Unknown";
static char s_temp_val_buf[24] = "—";
static char s_temp_note_buf[32] = "Data unavailable";
static char s_humid_val_buf[24] = "—";
static char s_humid_note_buf[32] = "Data unavailable";

static char s_dev_name_buf[UI_DEVICE_COUNT][24];
static char s_dev_state_buf[UI_DEVICE_COUNT][24];
static char s_dev_mode_buf[UI_DEVICE_COUNT][16];

static char s_toast_buf[64] = "";
static char s_time_axis_buf[4][16];


/* In-place Home page live widget pointers (P0.1, P1.15) */
static lv_obj_t *s_home_air_icon = NULL;
static lv_obj_t *s_home_air_status = NULL;
static lv_obj_t *s_home_air_badge = NULL;
static lv_obj_t *s_home_air_sub_lbl = NULL;
static char s_air_sub_buf[64] = "Based on eCO₂ and TVOC";
static lv_obj_t *s_home_co2_val = NULL;
static lv_obj_t *s_home_co2_unit_lbl = NULL;

static lv_obj_t *s_home_voc_val = NULL;
static lv_obj_t *s_home_voc_unit_lbl = NULL;

static lv_obj_t *s_home_temp_val = NULL;
static lv_obj_t *s_home_temp_unit_lbl = NULL;
static lv_obj_t *s_home_temp_note = NULL;
static lv_obj_t *s_home_humid_val = NULL;
static lv_obj_t *s_home_humid_unit_lbl = NULL;
static lv_obj_t *s_home_humid_note = NULL;

static lv_obj_t *s_home_dev_icons[UI_DEVICE_COUNT];
static lv_obj_t *s_home_dev_names[UI_DEVICE_COUNT];
static lv_obj_t *s_home_dev_states[UI_DEVICE_COUNT];
static lv_obj_t *s_home_dev_auto_icons[UI_DEVICE_COUNT];
static lv_obj_t *s_home_dev_modes[UI_DEVICE_COUNT];
static lv_obj_t *s_home_dev_dashes[UI_DEVICE_COUNT];
static lv_obj_t *s_home_dev_switches[UI_DEVICE_COUNT];
static lv_obj_t *s_home_dev_retries[UI_DEVICE_COUNT];

/* In-place Trends page live widget pointers */
static lv_obj_t *s_trends_unavail_badge = NULL;
static lv_obj_t *s_trends_chart = NULL;
static lv_chart_series_t *s_trends_chart_series = NULL;
static lv_obj_t *s_trends_y_lbls[3] = { NULL, NULL, NULL };
static char s_trends_y_buf[3][16];
static lv_obj_t *s_trends_empty_box = NULL;
static lv_obj_t *s_trends_empty_lbl = NULL;
static lv_obj_t *s_trends_empty_hint = NULL;
static lv_obj_t *s_trends_tab_btns[4];
static lv_obj_t *s_trends_tab_cards[4] = {NULL, NULL, NULL, NULL};
static lv_obj_t *s_trends_tab_icons[4] = {NULL, NULL, NULL, NULL};
static lv_obj_t *s_trends_tab_names[4] = {NULL, NULL, NULL, NULL};
static lv_obj_t *s_trends_tab_dots[4] = {NULL, NULL, NULL, NULL};
static lv_obj_t *s_trends_tab_vals[4] = {NULL, NULL, NULL, NULL};
static lv_obj_t *s_trends_tab_units[4] = {NULL, NULL, NULL, NULL};
static lv_obj_t *s_trends_title_lbl = NULL;
static lv_obj_t *s_trends_unit_lbl = NULL;
static char s_trends_unit_buf[16] = "";
static char s_trends_tab_val_buf[4][16] = {"—", "—", "—", "—"};
static char s_trends_tab_unit_buf[4][8] = {"", "", "", ""};
static lv_obj_t *s_trends_time_labels[4];

/* In-place Devices page live widget pointers */
static lv_obj_t *s_dev_page_icons[UI_DEVICE_COUNT];
static lv_obj_t *s_dev_page_names[UI_DEVICE_COUNT];
static lv_obj_t *s_dev_page_auto_icons[UI_DEVICE_COUNT];
static lv_obj_t *s_dev_page_modes[UI_DEVICE_COUNT];
static lv_obj_t *s_dev_page_states[UI_DEVICE_COUNT];
static lv_obj_t *s_dev_page_dashes[UI_DEVICE_COUNT];
static lv_obj_t *s_dev_page_switches[UI_DEVICE_COUNT];
static lv_obj_t *s_dev_page_retries[UI_DEVICE_COUNT];

/* Dialog mode mapping: Manual on left, Auto on right */
static const ui_device_mode_t s_dialog_btn_modes[2] = { UI_MODE_MANUAL, UI_MODE_AUTO };


/* Monotonic Uptime tracking */
static uint32_t s_uptime_seconds = 0u;
static uint32_t s_last_tick_ms = 0u;
static uint32_t s_ms_accumulator = 0u;

/* Forward Declarations */
static void do_navigate_to_page(ui_page_id_t page);
static void update_home_page_widgets(void);
static void update_trends_page_widgets(void);
static void update_devices_page_widgets(void);
static void update_header(void);
static void on_nav_btn_click(lv_event_t *e);
static void on_home_card_click(lv_event_t *e);
static void on_device_switch_click(lv_event_t *e);
static void on_device_retry_click(lv_event_t *e);
static void on_device_configure_click(lv_event_t *e);
static void open_device_dialog(uint8_t device_idx);
static void close_device_dialog(void);
static void build_dialog_device_subview(lv_obj_t *box);
static void build_dialog_auto_subview(lv_obj_t *box);
static void on_dialog_open_limits_click(lv_event_t *e);
static void on_dialog_limits_back_click(lv_event_t *e);
static void on_dialog_reset_limits_click(lv_event_t *e);
static void on_dialog_preset_change(lv_event_t *e);
static void on_dialog_preset_dropdown_event(lv_event_t *e);
static void on_dialog_polarity_btn_click(lv_event_t *e);
static void on_thresh_step_click(lv_event_t *e);
static void step_threshold(bool is_on, int32_t dir);
static void update_dialog_device_subview(void);
static void update_dialog_auto_subview(void);
static bool commit_device_config_internal(uint8_t idx, const ui_hub_device_config_t *cand, bool is_boot_staging, char *err_msg, size_t err_sz);
static void format_temp_c(char *buf, size_t buf_size, int32_t tenths);
static void reset_page_widget_pointers(void);

extern const lv_img_dsc_t ui_img_splash_logo;

/* Presets Table */
static const ui_preset_desc_t s_presets[UI_PRESET_COUNT] = {
    { "Air Purifier",    "purifier" },
    { "Ventilation Fan", "fan" },
    { "Humidifier",      "drop" },
    { "Dehumidifier",    "drop" },
    { "Heater",          "heat" },
    { "Desk Light",      "light" },
    { "Smart Socket",    "plug" },
    { "Generic Device",  "devices" }
};

static const lv_img_dsc_t *get_preset_icon_24(uint8_t preset)
{
    switch (preset) {
        case UI_PRESET_PURIFIER: return &ui_icon_purifier_24;
        case UI_PRESET_FAN: return &ui_icon_fan_24;
        case UI_PRESET_HUMIDIFIER:
        case UI_PRESET_DEHUMIDIFIER: return &ui_icon_drop_24;
        case UI_PRESET_HEATER: return &ui_icon_heat_24;
        case UI_PRESET_LIGHT: return &ui_icon_light_24;
        case UI_PRESET_PLUG: return &ui_icon_plug_24;
        default: return &ui_icon_plug_24;
    }
}

static const lv_img_dsc_t *get_preset_icon_32(uint8_t preset)
{
    switch (preset) {
        case UI_PRESET_PURIFIER: return &ui_icon_purifier_32;
        case UI_PRESET_FAN: return &ui_icon_fan_32;
        case UI_PRESET_HUMIDIFIER:
        case UI_PRESET_DEHUMIDIFIER: return &ui_icon_drop_32;
        case UI_PRESET_HEATER: return &ui_icon_heat_32;
        case UI_PRESET_LIGHT: return &ui_icon_light_32;
        case UI_PRESET_PLUG: return &ui_icon_plug_32;
        default: return &ui_icon_plug_32;
    }
}

static const lv_img_dsc_t *get_preset_icon_42(uint8_t preset)
{
    switch (preset) {
        case UI_PRESET_PURIFIER: return &ui_icon_purifier_42;
        case UI_PRESET_FAN: return &ui_icon_fan_42;
        case UI_PRESET_HUMIDIFIER:
        case UI_PRESET_DEHUMIDIFIER: return &ui_icon_drop_42;
        case UI_PRESET_HEATER: return &ui_icon_heat_42;
        case UI_PRESET_LIGHT: return &ui_icon_light_42;
        case UI_PRESET_PLUG: return &ui_icon_plug_42;
        default: return &ui_icon_plug_42;
    }
}

static const lv_img_dsc_t *get_preset_icon_44(uint8_t preset)
{
    switch (preset) {
        case UI_PRESET_PURIFIER: return &ui_icon_purifier_44;
        case UI_PRESET_FAN: return &ui_icon_fan_44;
        case UI_PRESET_HUMIDIFIER:
        case UI_PRESET_DEHUMIDIFIER: return &ui_icon_drop_44;
        case UI_PRESET_HEATER: return &ui_icon_heat_44;
        case UI_PRESET_LIGHT: return &ui_icon_light_44;
        case UI_PRESET_PLUG: return &ui_icon_plug_44;
        default: return &ui_icon_plug_44;
    }
}

/* =========================================================================
 * Helper Functions
 * ========================================================================= */
static lv_obj_t *add_static_label(lv_obj_t *parent, const char *text,
                                  const lv_font_t *font, lv_color_t color)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text_static(label, text);
    if (font == &ui_font_18 && color.full == UI_COLOR_INK.full) {
        lv_obj_add_style(label, &ui_style_lbl_18_ink, 0);
    } else if (font == &ui_font_24 && color.full == UI_COLOR_INK.full) {
        lv_obj_add_style(label, &ui_style_lbl_24_ink, 0);
    } else if (font == &ui_font_18 && color.full == UI_COLOR_MUTED.full) {
        lv_obj_add_style(label, &ui_style_lbl_18_muted, 0);
    } else if (font == &ui_font_18 && color.full == UI_COLOR_RED.full) {
        lv_obj_add_style(label, &ui_style_lbl_18_red, 0);
    } else if (font == &ui_font_24 && color.full == lv_color_white().full) {
        lv_obj_add_style(label, &ui_style_lbl_24_white, 0);
    } else if (font == &ui_font_18 && color.full == UI_COLOR_BLUE.full) {
        lv_obj_add_style(label, &ui_style_lbl_18_blue, 0);
    } else if (font == &ui_font_18 && color.full == UI_COLOR_BLUE_TEXT.full) {
        lv_obj_add_style(label, &ui_style_lbl_18_blue_text, 0);
    } else if (font == &ui_font_24 && color.full == UI_COLOR_AMBER.full) {
        lv_obj_add_style(label, &ui_style_lbl_24_amber, 0);
    } else if (font == &ui_font_24 && color.full == UI_COLOR_MUTED.full) {
        lv_obj_add_style(label, &ui_style_lbl_24_muted, 0);
    } else if (font == &ui_font_34 && color.full == UI_COLOR_INK.full) {
        lv_obj_add_style(label, &ui_style_lbl_34_ink, 0);
    } else if (font == &ui_font_48 && color.full == lv_color_white().full) {
        lv_obj_add_style(label, &ui_style_lbl_48_white, 0);
    } else if (font == &ui_font_48 && color.full == UI_COLOR_INK.full) {
        lv_obj_add_style(label, &ui_style_lbl_48_ink, 0);
    } else {
        lv_obj_set_style_text_font(label, font, LV_PART_MAIN);
        lv_obj_set_style_text_color(label, color, LV_PART_MAIN);
    }
    return label;
}

static lv_obj_t *add_icon(lv_obj_t *parent, const lv_img_dsc_t *src, lv_color_t color)
{
    lv_obj_t *img = lv_img_create(parent);
    lv_img_set_src(img, src);
    if (color.full == UI_COLOR_ICON_BLUE.full) {
        lv_obj_add_style(img, &ui_style_icon_blue, 0);
    } else if (color.full == UI_COLOR_MUTED.full) {
        lv_obj_add_style(img, &ui_style_icon_muted, 0);
    } else if (color.full == lv_color_white().full) {
        lv_obj_add_style(img, &ui_style_icon_white, 0);
    } else {
        lv_obj_set_style_img_recolor(img, color, 0);
        lv_obj_set_style_img_recolor_opa(img, LV_OPA_COVER, 0);
    }
    return img;
}

static void format_temp_c(char *buf, size_t buf_size, int32_t tenths)
{
    int64_t t = tenths;
    int64_t abs_tenths = (t < 0) ? -t : t;
    snprintf(buf, buf_size, "%s%ld.%ld", (t < 0) ? "-" : "",
             (long)(abs_tenths / 10), (long)(abs_tenths % 10));
}

static void format_metric_number(char *buf, size_t size, ui_metric_id_t metric, int32_t val, bool valid)
{
    if (!valid) {
        snprintf(buf, size, "—");
        return;
    }
    switch (metric) {
        case UI_METRIC_TEMP: {
            format_temp_c(buf, size, val);
            break;
        }
        default:
            snprintf(buf, size, "%d", (int)val);
            break;
    }
}

static const char *get_metric_unit_str(ui_metric_id_t metric)
{
    switch (metric) {
        case UI_METRIC_CO2: return "ppm";
        case UI_METRIC_TEMP: return "°C";
        case UI_METRIC_HUMIDITY: return "%";
        case UI_METRIC_VOC: return "ppb";
        default: return "";
    }
}

static const char *get_device_display_name(const ui_hub_device_config_t *cfg)
{
    if (cfg == NULL) return "";
    if (cfg->preset == UI_PRESET_FAN && strcmp(cfg->name, "Ventilation Fan") == 0) {
        return "Ventilation";
    }
    return cfg->name;
}

/* =========================================================================
 * Change-Guard Helpers (P1.4 Redundant Invalidation Elimination)
 * ========================================================================= */
static inline bool ui_obj_has_style(const lv_obj_t *obj, const lv_style_t *style, lv_style_selector_t selector)
{
    if (obj == NULL || style == NULL || obj->styles == NULL) return false;
    uint32_t cnt = obj->style_cnt;
    for (uint32_t i = 0; i < cnt; i++) {
        if (obj->styles[i].style == style && obj->styles[i].selector == selector) {
            return true;
        }
    }
    return false;
}

static inline void ui_obj_add_style_once(lv_obj_t *obj, const lv_style_t *style, lv_style_selector_t selector)
{
    if (obj == NULL || style == NULL) return;
    if (!ui_obj_has_style(obj, style, selector)) {
        lv_obj_add_style(obj, style, selector);
    }
}

static inline void set_font_if_changed(lv_obj_t *obj, const lv_font_t *font, lv_style_selector_t selector)
{
    if (obj == NULL || font == NULL) return;
    const lv_font_t *cur = lv_obj_get_style_text_font(obj, selector);
    if (cur != font) {
        lv_obj_set_style_text_font(obj, font, selector);
    }
}

/* Unified read-only sensor availability helper (R5) */
static bool ui_is_sensor_available(ui_metric_id_t metric, uint32_t now)
{
    if (!is_link_connected(now)) return false;
    if (lora_node_status.bits.rs485_fault) return false;
    switch (metric) {
        case UI_METRIC_CO2:
            if (lora_node_status.bits.co2_timeout) return false;
            if (!lora_node_status.bits.co2_valid) return false;
            if (lora_node_status.bits.co2_ppm < 400U ||
                lora_node_status.bits.co2_ppm > UI_AUTO_CO2_MAX) return false;
            return true;
        case UI_METRIC_VOC:
            if (lora_node_status.bits.co2_timeout) return false;
            if (!lora_node_status.bits.voc_valid) return false;
            if (lora_node_status.bits.tvoc_ppb > UI_AUTO_VOC_MAX) return false;
            return true;
        case UI_METRIC_TEMP:
            if (lora_node_status.bits.th_timeout) return false;
            if (!lora_node_status.bits.temp_valid) return false;
            return true;
        case UI_METRIC_HUMIDITY:
            if (lora_node_status.bits.th_timeout) return false;
            if (!lora_node_status.bits.humid_valid) return false;
            if (lora_node_status.bits.humidity_pct > 100) return false;
            return true;
        default:
            return false;
    }
}

static void push_history_sample(ui_metric_id_t metric, uint32_t minute, bool valid, int32_t val)
{
    if (metric >= UI_METRIC_COUNT) return;
    if (s_history_count[metric] < UI_HISTORY_CAPACITY) {
        uint16_t idx = s_history_count[metric]++;
        s_history[metric][idx].minute_of_day = minute;
        s_history[metric][idx].valid = valid;
        s_history[metric][idx].value = val;
    } else {
        memmove(&s_history[metric][0], &s_history[metric][1], (UI_HISTORY_CAPACITY - 1) * sizeof(ui_history_sample_t));
        s_history[metric][UI_HISTORY_CAPACITY - 1].minute_of_day = minute;
        s_history[metric][UI_HISTORY_CAPACITY - 1].valid = valid;
        s_history[metric][UI_HISTORY_CAPACITY - 1].value = val;
    }
}

static inline int32_t get_live_metric_raw_value(ui_metric_id_t m)
{
    switch (m) {
        case UI_METRIC_CO2: return lora_node_status.bits.co2_ppm;
        case UI_METRIC_VOC: return lora_node_status.bits.tvoc_ppb;
        case UI_METRIC_TEMP: return lora_node_status.bits.temp_deci_c;
        case UI_METRIC_HUMIDITY: return lora_node_status.bits.humidity_pct;
        default: return 0;
    }
}


static inline bool set_buffer_and_label_if_changed(lv_obj_t *label, char *buf, size_t buf_sz, const char *new_text)
{
    if (buf == NULL || new_text == NULL || buf_sz == 0) return false;
    if (strcmp(buf, new_text) != 0) {
        size_t len = strlen(new_text);
        if (len >= buf_sz) len = buf_sz - 1;
        memcpy(buf, new_text, len);
        buf[len] = '\0';
        if (label != NULL) {
            lv_label_set_text_static(label, buf);
        }
        return true;
    }
    return false;
}

static inline bool update_label_if_changed(lv_obj_t *label, const char *new_text)
{
    if (label == NULL || new_text == NULL) return false;
    const char *cur = lv_label_get_text(label);
    if (cur == NULL || strcmp(cur, new_text) != 0) {
        lv_label_set_text_static(label, new_text);
        return true;
    }
    return false;
}

static inline void set_text_color_if_changed(lv_obj_t *obj, lv_color_t color)
{
    if (obj == NULL) return;
    lv_color_t cur = lv_obj_get_style_text_color(obj, 0);
    if (cur.full != color.full) {
        lv_obj_set_style_text_color(obj, color, 0);
    }
}

static inline void set_bg_color_if_changed(lv_obj_t *obj, lv_color_t color)
{
    if (obj == NULL) return;
    lv_color_t cur = lv_obj_get_style_bg_color(obj, 0);
    if (cur.full != color.full) {
        lv_obj_set_style_bg_color(obj, color, 0);
    }
}

static inline void set_bg_opa_if_changed(lv_obj_t *obj, lv_opa_t opa)
{
    if (obj == NULL) return;
    lv_opa_t cur = lv_obj_get_style_bg_opa(obj, 0);
    if (cur != opa) {
        lv_obj_set_style_bg_opa(obj, opa, 0);
    }
}

static inline void set_flag_if_changed(lv_obj_t *obj, lv_obj_flag_t flag, bool set)
{
    if (obj == NULL) return;
    bool has = lv_obj_has_flag(obj, flag);
    if (set && !has) {
        lv_obj_add_flag(obj, flag);
    } else if (!set && has) {
        lv_obj_clear_flag(obj, flag);
    }
}

static inline void set_state_if_changed(lv_obj_t *obj, lv_state_t state, bool set)
{
    if (obj == NULL) return;
    bool has = lv_obj_has_state(obj, state);
    if (set && !has) {
        lv_obj_add_state(obj, state);
    } else if (!set && has) {
        lv_obj_clear_state(obj, state);
    }
}

static inline void set_pos_if_changed(lv_obj_t *obj, lv_coord_t x, lv_coord_t y)
{
    if (obj == NULL) return;
    if (lv_obj_get_x(obj) != x || lv_obj_get_y(obj) != y) {
        lv_obj_set_pos(obj, x, y);
    }
}

static inline void set_img_src_if_changed(lv_obj_t *obj, const void *src)
{
    if (obj == NULL || src == NULL) return;
    if (lv_img_get_src(obj) != src) {
        lv_img_set_src(obj, src);
    }
}

static inline void set_img_recolor_if_changed(lv_obj_t *obj, lv_color_t color)
{
    if (obj == NULL) return;
    lv_color_t cur = lv_obj_get_style_img_recolor(obj, 0);
    if (cur.full != color.full) {
        lv_obj_set_style_img_recolor(obj, color, 0);
    }
}

static void update_uptime_string(void)
{
    uint32_t hours = s_uptime_seconds / 3600u;
    uint32_t minutes = (s_uptime_seconds / 60u) % 60u;
    uint32_t seconds = s_uptime_seconds % 60u;
    snprintf(s_uptime_buf, sizeof(s_uptime_buf), "%02u:%02u:%02u",
             (unsigned)hours, (unsigned)minutes, (unsigned)seconds);
}

const char *ui_get_uptime_str(void)
{
    update_uptime_string();
    return s_uptime_buf;
}

uint8_t ui_get_link_bars(void)
{
    return s_current_link_bars;
}

const char *ui_get_link_status_str(void)
{
    if (s_link_label == NULL) return "";
    return lv_label_get_text(s_link_label);
}

/* =========================================================================
 * Toast Announcement (P1.12 Bounded Timer)
 * ========================================================================= */
static void toast_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    if (s_toast_obj != NULL) {
        lv_obj_add_flag(s_toast_obj, LV_OBJ_FLAG_HIDDEN);
    }
    s_toast_timer = NULL; /* LVGL auto-deletes repeat_count=1 timer */
    s_toast_buf[0] = '\0';
}

void ui_announce(const char *message)
{
    if (message == NULL || s_toast_obj == NULL || s_dialog_overlay != NULL) return;
    snprintf(s_toast_buf, sizeof(s_toast_buf), "%s", message);
    lv_label_set_text_static(s_toast_label, s_toast_buf);
    lv_obj_clear_flag(s_toast_obj, LV_OBJ_FLAG_HIDDEN);
    lv_obj_align(s_toast_obj, LV_ALIGN_TOP_RIGHT, -24, 60);

    if (s_toast_timer != NULL) {
        lv_timer_del(s_toast_timer);
        s_toast_timer = NULL;
    }
    s_toast_timer = lv_timer_create(toast_timer_cb, 2600u, NULL);
    lv_timer_set_repeat_count(s_toast_timer, 1);
}

void ui_dismiss_toast(void)
{
    if (s_toast_obj != NULL) {
        lv_obj_add_flag(s_toast_obj, LV_OBJ_FLAG_HIDDEN);
    }
    if (s_toast_timer != NULL) {
        lv_timer_del(s_toast_timer);
        s_toast_timer = NULL;
    }
    s_toast_buf[0] = '\0';
}

/* =========================================================================
 * Top Header Construction (Collapsed wrappers - P0.1)
 * ========================================================================= */
static void build_header(void)
{
    s_header = lv_obj_create(s_screen);
    lv_obj_remove_style_all(s_header);
    lv_obj_set_size(s_header, 800, 56);
    lv_obj_set_pos(s_header, 0, 0);
    lv_obj_clear_flag(s_header, LV_OBJ_FLAG_SCROLLABLE);

    /* Left: Home Icon + Studio label placed directly on header */
    lv_obj_t *home_ico = add_icon(s_header, &ui_icon_home_32, UI_COLOR_INK);
    lv_obj_set_pos(home_ico, 24, 15);

    lv_obj_t *studio_lbl = add_static_label(s_header, "Studio", &ui_font_34, UI_COLOR_INK);
    lv_obj_set_pos(studio_lbl, 64, 8);

    /* Right cluster: Signal bars indicator + Link label */
    s_link_bars_cont = lv_obj_create(s_header);
    lv_obj_remove_style_all(s_link_bars_cont);
    lv_obj_set_size(s_link_bars_cont, 22, 18);
    lv_obj_clear_flag(s_link_bars_cont, LV_OBJ_FLAG_SCROLLABLE);

    static const lv_coord_t s_bar_heights[4] = {5, 9, 13, 18};
    for (int i = 0; i < 4; i++) {
        s_link_bars[i] = lv_obj_create(s_link_bars_cont);
        lv_obj_remove_style_all(s_link_bars[i]);
        lv_obj_set_size(s_link_bars[i], 4, s_bar_heights[i]);
        lv_obj_set_pos(s_link_bars[i], (lv_coord_t)(i * 6), (lv_coord_t)(18 - s_bar_heights[i]));
        lv_obj_set_style_bg_color(s_link_bars[i], UI_COLOR_SWITCH_OFF, 0);
        lv_obj_set_style_bg_opa(s_link_bars[i], LV_OPA_COVER, 0);
    }
    s_current_link_bars = 0;

    s_link_label = add_static_label(s_header, "Disconnected", &ui_font_24, UI_COLOR_INK);

    update_header();
}

static void update_header(void)
{
    if (s_header == NULL || s_link_label == NULL) return;

    static bool s_header_initialized = false;
    static bool s_last_link_connected = false;

    uint32_t now = lv_tick_get();
    bool link_connected = is_link_connected(now);
    bool link_changed = false;

    uint8_t target_bars = 0;
    if (link_connected) {
        target_bars = lora_snr_to_bars(lora_last_snr, s_current_link_bars);
        if (target_bars == 0) {
            /* Floor: status frames are arriving within staleness bound, so link is connected.
             * Clamp to at least 1 bar even when SNR margin is negative or below demodulator limit. */
            target_bars = 1;
        }
        link_changed = update_label_if_changed(s_link_label, "Connected");
        set_text_color_if_changed(s_link_label, UI_COLOR_INK);
    } else {
        target_bars = 0;
        link_changed = update_label_if_changed(s_link_label, "Disconnected");
        set_text_color_if_changed(s_link_label, UI_COLOR_RED);
    }

    if (target_bars != s_current_link_bars || !s_header_initialized) {
        s_current_link_bars = target_bars;
        link_changed = true;
        for (int i = 0; i < 4; i++) {
            if (s_link_bars[i] != NULL) {
                lv_color_t color = (i < target_bars) ? UI_COLOR_GREEN : UI_COLOR_SWITCH_OFF;
                set_bg_color_if_changed(s_link_bars[i], color);
            }
        }
    }

    if (!s_header_initialized || link_changed || link_connected != s_last_link_connected) {
        s_header_initialized = true;
        s_last_link_connected = link_connected;

        /* Ensure link label layout is resolved so width reflects current text (G1). */
        lv_obj_update_layout(s_link_label);
        lv_coord_t link_w = lv_obj_get_width(s_link_label);

        lv_coord_t link_lbl_x = 778 - link_w;
        lv_coord_t link_ico_x = link_lbl_x - 10 - 22;

        set_pos_if_changed(s_link_label, link_lbl_x, 13);
        set_pos_if_changed(s_link_bars_cont, link_ico_x, 19);
    }
}

/* =========================================================================
 * Top-level navigation (Canonical separate row at y=56..100)
 * ========================================================================= */
static void update_navigation_selection(void)
{
    for (int i = 0; i < 3; i++) {
        bool selected = (i == (int)s_current_page);
        set_bg_color_if_changed(s_nav_btns[i], selected ? UI_COLOR_CELADON : lv_color_make(0, 0, 0));
        set_bg_opa_if_changed(s_nav_btns[i], selected ? LV_OPA_COVER : LV_OPA_TRANSP);
        set_text_color_if_changed(s_nav_icons[i], selected ? UI_COLOR_INK : UI_COLOR_MUTED);
    }
}

static void on_nav_btn_click(lv_event_t *e)
{
    uintptr_t page = (uintptr_t)lv_event_get_user_data(e);
    if ((ui_page_id_t)page == s_current_page) return;
    ui_navigate_to_page((ui_page_id_t)page);
}

static void build_navigation(void)
{
    s_nav_bar = lv_obj_create(s_screen);
    lv_obj_remove_style_all(s_nav_bar);
    lv_obj_set_size(s_nav_bar, 752, 44);
    lv_obj_set_pos(s_nav_bar, 24, 56);
    lv_obj_clear_flag(s_nav_bar, LV_OBJ_FLAG_SCROLLABLE);

    static const char *names[] = { "Overview", "Trends", "Devices" };
    static const lv_coord_t s_nav_widths[3] = { 120, 100, 110 };
    static const lv_coord_t s_nav_x_pos[3] = { 0, 132, 244 };

    for (uintptr_t i = 0; i < 3; i++) {
        lv_obj_t *btn = lv_btn_create(s_nav_bar);
        lv_obj_remove_style_all(btn);
        lv_obj_set_pos(btn, s_nav_x_pos[i], 4);
        lv_obj_set_size(btn, s_nav_widths[i], 36);
        lv_obj_set_style_radius(btn, 18, 0);
        lv_obj_set_ext_click_area(btn, 6); /* Guarantees >= 48px touch target (36 + 2*6 = 48) */
        lv_obj_add_style(btn, &ui_style_nav_btn_pressed, LV_STATE_PRESSED);
        lv_obj_add_event_cb(btn, on_nav_btn_click, LV_EVENT_CLICKED, (void *)i);
        s_nav_btns[i] = btn;
        s_nav_icons[i] = add_static_label(btn, names[i], &ui_font_18, UI_COLOR_MUTED);
        lv_obj_align(s_nav_icons[i], LV_ALIGN_CENTER, 0, -2);
    }
}

/* =========================================================================
 * Toast Container
 * ========================================================================= */
static void build_toast(void)
{
    s_toast_obj = lv_obj_create(s_screen);
    lv_obj_remove_style_all(s_toast_obj);
    lv_obj_set_size(s_toast_obj, LV_SIZE_CONTENT, 40);
    lv_obj_set_style_bg_color(s_toast_obj, UI_COLOR_BLUE, 0);
    lv_obj_set_style_bg_opa(s_toast_obj, 230, 0);
    lv_obj_set_style_radius(s_toast_obj, 20, 0);
    lv_obj_set_style_pad_hor(s_toast_obj, 20, 0);
    lv_obj_set_style_pad_ver(s_toast_obj, 7, 0);
    lv_obj_add_flag(s_toast_obj, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(s_toast_obj, LV_OBJ_FLAG_CLICKABLE);

    s_toast_label = add_static_label(s_toast_obj, s_toast_buf, &ui_font_18, lv_color_white());
}

/* =========================================================================
 * HOME PAGE (Collapsed flat hierarchy - P0.1)
 * ========================================================================= */
static void on_home_card_click(lv_event_t *e)
{
    uintptr_t metric = (uintptr_t)lv_event_get_user_data(e);
    ui_select_trend_metric((ui_metric_id_t)metric);
}

static void set_quality_label(lv_obj_t *label, ui_quality_category_t quality,
                              char *buffer, size_t size)
{
    static const char *const names[] = { "Good", "Moderate", "Poor" };
    const char *text = quality == UI_QUALITY_GOOD ? names[0] :
                       quality == UI_QUALITY_MODERATE ? names[1] :
                       quality == UI_QUALITY_POOR ? names[2] : "Unknown";
    set_buffer_and_label_if_changed(label, buffer, size, text);
}

static const char *get_device_mode_display_string(uint8_t idx, bool offline)
{
    if (idx >= UI_DEVICE_COUNT) return "";
    const ui_hub_device_config_t *cfg = &s_dev_configs[idx];
    if (cfg->mode != UI_MODE_AUTO) {
        return "Manual";
    }
    if (offline || s_auto_states[idx].paused_error || s_auto_states[idx].suspended_sensor) {
        return "Auto paused";
    }
    switch (cfg->preset) {
        case UI_PRESET_FAN: return "Auto · eCO₂";
        case UI_PRESET_PURIFIER: return "Auto · TVOC";
        case UI_PRESET_HUMIDIFIER: return "Auto · RH";
        default: return "Auto";
    }
}

static void build_home_page(void)
{
    lv_obj_t *p = s_page_container;

    /* 1. Air quality Forest Hero Card (w: 416, h: 232 at x: 0, y: 0) */
    lv_obj_t *hero = lv_obj_create(p);
    lv_obj_remove_style_all(hero);
    lv_obj_set_pos(hero, 0, 0);
    lv_obj_set_size(hero, 416, 232);
    lv_obj_add_style(hero, &ui_style_card_hero, 0);
    lv_obj_clear_flag(hero, LV_OBJ_FLAG_SCROLLABLE);

    /* "Air quality" title (18px, celadon) */
    lv_obj_t *title = add_static_label(hero, "Air quality", &ui_font_18, UI_COLOR_CELADON);
    lv_obj_set_pos(title, 20, 14);

    /* Air glyph outline icon (32px, celadon) at top right */
    s_home_air_icon = add_icon(hero, &ui_icon_air_32, UI_COLOR_CELADON);
    lv_obj_set_pos(s_home_air_icon, 364, 14);

    /* Verdict (48px, white) e.g. "Good", "Moderate", "Poor", "Unavailable" */
    s_home_air_status = add_static_label(hero, "Good", &ui_font_48, lv_color_white());
    lv_obj_set_pos(s_home_air_status, 20, 38);

    /* Celadon check badge next to "Good" */
    s_home_air_badge = add_icon(hero, &ui_icon_check_circle_32, UI_COLOR_CELADON);
    lv_obj_set_pos(s_home_air_badge, 178, 53);

    /* Subtitle (18px, celadon) */
    s_home_air_sub_lbl = add_static_label(hero, s_air_sub_buf, &ui_font_18, UI_COLOR_CELADON);
    lv_obj_set_pos(s_home_air_sub_lbl, 20, 104);

    /* Subtle divider line */
    lv_obj_t *h_div = lv_obj_create(hero);
    lv_obj_remove_style_all(h_div);
    lv_obj_set_pos(h_div, 20, 138);
    lv_obj_set_size(h_div, 376, 1);
    lv_obj_set_style_bg_color(h_div, lv_color_hex(0x2A4D3D), 0);
    lv_obj_set_style_bg_opa(h_div, LV_OPA_COVER, 0);

    /* Bottom row: CO2 column and VOC column */
    /* CO2 Column (clickable -> Trends CO2) */
    lv_obj_t *co2_col = lv_obj_create(hero);
    lv_obj_remove_style_all(co2_col);
    lv_obj_set_pos(co2_col, 20, 142);
    lv_obj_set_size(co2_col, 176, 76);
    lv_obj_clear_flag(co2_col, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(co2_col, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(co2_col, on_home_card_click, LV_EVENT_CLICKED, (void *)(uintptr_t)0);

    lv_obj_t *co2_lbl = add_static_label(co2_col, "eCO₂", &ui_font_18, UI_COLOR_CELADON);
    lv_obj_set_pos(co2_lbl, 0, 2);

    s_home_co2_val = add_static_label(co2_col, s_co2_val_buf, &ui_font_34, lv_color_white());
    lv_obj_set_pos(s_home_co2_val, 0, 26);

    s_home_co2_unit_lbl = add_static_label(co2_col, "ppm", &ui_font_18, UI_COLOR_CELADON);
    lv_obj_set_pos(s_home_co2_unit_lbl, 86, 43);

    /* Vertical separator between CO2 and VOC (calibrated to center x=207, symmetrical 11px padding) */
    lv_obj_t *v_div = lv_obj_create(hero);
    lv_obj_remove_style_all(v_div);
    lv_obj_set_pos(v_div, 207, 146);
    lv_obj_set_size(v_div, 1, 64);
    lv_obj_set_style_bg_color(v_div, lv_color_hex(0x2A4D3D), 0);
    lv_obj_set_style_bg_opa(v_div, LV_OPA_COVER, 0);

    /* TVOC column; the source sensor reports concentration in ppb. */
    lv_obj_t *voc_col = lv_obj_create(hero);
    lv_obj_remove_style_all(voc_col);
    lv_obj_set_pos(voc_col, 219, 142);
    lv_obj_set_size(voc_col, 177, 76);
    lv_obj_clear_flag(voc_col, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(voc_col, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(voc_col, on_home_card_click, LV_EVENT_CLICKED, (void *)(uintptr_t)1);

    lv_obj_t *voc_lbl = add_static_label(voc_col, "TVOC", &ui_font_18, UI_COLOR_CELADON);
    lv_obj_set_pos(voc_lbl, 0, 2);

    s_home_voc_val = add_static_label(voc_col, s_voc_val_buf, &ui_font_34, lv_color_white());
    lv_obj_set_pos(s_home_voc_val, 0, 26);

    s_home_voc_unit_lbl = add_static_label(voc_col, "ppb", &ui_font_18, UI_COLOR_CELADON);
    lv_obj_set_pos(s_home_voc_unit_lbl, 86, 43);

    /* 2. Lower Shallow Environmental Tiles: Temperature (200x96 at 0, 248) and Humidity (200x96 at 216, 248) */
    for (uint8_t i = 0; i < 2; i++) {
        lv_obj_t *card = lv_obj_create(p);
        lv_obj_remove_style_all(card);
        lv_obj_set_pos(card, i ? 216 : 0, 248);
        lv_obj_set_size(card, 200, 96);
        lv_obj_add_style(card, &ui_style_card_porcelain, 0);
        lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(card, on_home_card_click, LV_EVENT_CLICKED, (void *)(uintptr_t)(i + 2));

        lv_obj_t *ico = add_icon(card, i ? &ui_icon_drop_32 : &ui_icon_thermometer_32, UI_COLOR_FOREST);
        lv_obj_set_pos(ico, 16, 18);

        lv_obj_t *t_lbl = add_static_label(card, i ? "Humidity" : "Temperature", &ui_font_18, UI_COLOR_MUTED);
        lv_obj_set_pos(t_lbl, 60, 14);

        lv_obj_t *val_lbl = add_static_label(card, i ? s_humid_val_buf : s_temp_val_buf, &ui_font_34, UI_COLOR_INK);
        lv_obj_set_pos(val_lbl, 60, 40);

        lv_obj_t *unit_lbl = add_static_label(card, i ? "%" : "°C", &ui_font_18, UI_COLOR_INK);
        lv_obj_set_pos(unit_lbl, 60 + 50, 57);

        lv_obj_t *note_lbl = add_static_label(card, i ? s_humid_note_buf : s_temp_note_buf, &ui_font_18, UI_COLOR_MUTED);
        lv_obj_set_pos(note_lbl, 100, 56);
        lv_obj_add_flag(note_lbl, LV_OBJ_FLAG_HIDDEN);

        if (i) {
            s_home_humid_val = val_lbl;
            s_home_humid_unit_lbl = unit_lbl;
            s_home_humid_note = note_lbl;
        } else {
            s_home_temp_val = val_lbl;
            s_home_temp_unit_lbl = unit_lbl;
            s_home_temp_note = note_lbl;
        }
    }

    /* 3. Right Devices Panel (320x344 at 432, 0) */
    lv_obj_t *panel = lv_obj_create(p);
    lv_obj_remove_style_all(panel);
    lv_obj_set_pos(panel, 432, 0);
    lv_obj_set_size(panel, 320, 344);
    lv_obj_add_style(panel, &ui_style_card_porcelain, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

    /* Section Title */
    lv_obj_t *p_title = add_static_label(panel, "Devices", &ui_font_24, UI_COLOR_INK);
    lv_obj_set_pos(p_title, 20, 16);

    /* 44x44 shortcut button to Devices page */
    lv_obj_t *btn_all = lv_btn_create(panel);
    lv_obj_remove_style_all(btn_all);
    lv_obj_set_pos(btn_all, 256, 8);
    lv_obj_set_size(btn_all, 44, 44);
    lv_obj_add_style(btn_all, &ui_style_pressed, LV_STATE_PRESSED);
    lv_obj_add_flag(btn_all, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(btn_all, on_nav_btn_click, LV_EVENT_CLICKED, (void *)(uintptr_t)UI_PAGE_DEVICES);
    lv_obj_t *ico_all = add_icon(btn_all, &ui_icon_sliders_24, UI_COLOR_INK);
    lv_obj_center(ico_all);

    /* 4 Compact Device Rows */
    for (uint8_t i = 0; i < UI_DEVICE_COUNT; i++) {
        lv_obj_t *tile = lv_obj_create(panel);
        lv_obj_remove_style_all(tile);
        lv_obj_set_pos(tile, 10, 56 + i * 70);
        lv_obj_set_size(tile, 300, 70);
        lv_obj_clear_flag(tile, LV_OBJ_FLAG_SCROLLABLE);

        s_home_dev_icons[i] = add_icon(tile, get_preset_icon_32(s_dev_configs[i].preset), UI_COLOR_INK);
        lv_obj_set_pos(s_home_dev_icons[i], 8, 19);

        s_home_dev_names[i] = add_static_label(tile, s_dev_name_buf[i], &ui_font_18, UI_COLOR_INK);
        lv_obj_set_pos(s_home_dev_names[i], 52, 10);
        lv_obj_set_width(s_home_dev_names[i], 176);
        lv_label_set_long_mode(s_home_dev_names[i], LV_LABEL_LONG_DOT);

        s_home_dev_auto_icons[i] = add_icon(tile, &ui_icon_leaf_18, UI_COLOR_FOREST);
        lv_obj_set_pos(s_home_dev_auto_icons[i], 52, 38);
        lv_obj_add_flag(s_home_dev_auto_icons[i], LV_OBJ_FLAG_HIDDEN);

        s_home_dev_modes[i] = add_static_label(tile, s_dev_mode_buf[i], &ui_font_18, UI_COLOR_MUTED);
        lv_obj_set_pos(s_home_dev_modes[i], 52, 36);
        lv_obj_set_width(s_home_dev_modes[i], 176);
        lv_label_set_long_mode(s_home_dev_modes[i], LV_LABEL_LONG_CLIP);

        s_home_dev_states[i] = add_static_label(tile, s_dev_state_buf[i], &ui_font_18, UI_COLOR_RED);
        lv_obj_set_pos(s_home_dev_states[i], 168, 36);

        s_home_dev_dashes[i] = add_static_label(tile, "—", &ui_font_24, UI_COLOR_MUTED);
        lv_obj_set_pos(s_home_dev_dashes[i], 250, 18);

        s_home_dev_switches[i] = lv_switch_create(tile);
        lv_obj_set_size(s_home_dev_switches[i], 54, 30);
        lv_obj_set_pos(s_home_dev_switches[i], 240, 18);
        lv_obj_set_ext_click_area(s_home_dev_switches[i], 7);
        lv_obj_add_style(s_home_dev_switches[i], &ui_style_switch_main, LV_PART_MAIN);
        lv_obj_add_style(s_home_dev_switches[i], &ui_style_switch_indicator, LV_PART_INDICATOR | LV_STATE_CHECKED);
        lv_obj_add_event_cb(s_home_dev_switches[i], on_device_switch_click, LV_EVENT_VALUE_CHANGED, (void *)(uintptr_t)i);

        s_home_dev_retries[i] = add_static_label(tile, "Retry", &ui_font_18, UI_COLOR_BLUE);
        lv_obj_set_pos(s_home_dev_retries[i], 244, 11);
        lv_obj_set_size(s_home_dev_retries[i], 52, 44);
        lv_obj_set_ext_click_area(s_home_dev_retries[i], 0);
        lv_obj_add_flag(s_home_dev_retries[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(s_home_dev_retries[i], on_device_retry_click, LV_EVENT_CLICKED, (void *)(uintptr_t)i);

        if (i < UI_DEVICE_COUNT - 1) {
            lv_obj_t *div = lv_obj_create(panel);
            lv_obj_remove_style_all(div);
            lv_obj_set_pos(div, 12, 56 + (i + 1) * 70 - 2);
            lv_obj_set_size(div, 296, 1);
            lv_obj_set_style_bg_color(div, UI_COLOR_DIVIDER, 0);
            lv_obj_set_style_bg_opa(div, LV_OPA_COVER, 0);
        }
    }
    update_home_page_widgets();
}

/* Air-quality bands (upper bounds are inclusive). eCO2 in ppm. TVOC in ppb follows the German UBA
 * TVOC levels as published for SGP30-class ethanol-equivalent sensors, whose range/accuracy table the
 * MXVC10S matches: levels 1-2 (<= 220 ppb) Good, level 3 (<= 660 ppb) Moderate, levels 4-5 Poor. */
#define UI_ECO2_GOOD_MAX_PPM      800u
#define UI_ECO2_MODERATE_MAX_PPM  1200u
#define UI_TVOC_GOOD_MAX_PPB      220u
#define UI_TVOC_MODERATE_MAX_PPB  660u

static void update_home_page_widgets(void)
{
    if (s_current_page != UI_PAGE_HOME || s_home_co2_val == NULL) return;

    uint32_t now = lv_tick_get();
    bool offline = !is_link_connected(now);
    bool co2_avail = ui_is_sensor_available(UI_METRIC_CO2, now);
    bool voc_avail = ui_is_sensor_available(UI_METRIC_VOC, now);
    bool temp_avail = ui_is_sensor_available(UI_METRIC_TEMP, now);
    bool humid_avail = ui_is_sensor_available(UI_METRIC_HUMIDITY, now);

    const char *air = "Unavailable";
    const char *sub = "Sensor link disconnected";
    if (co2_avail && voc_avail) {
        uint16_t co2 = lora_node_status.bits.co2_ppm;
        uint16_t voc = lora_node_status.bits.tvoc_ppb;
        if (co2 > UI_ECO2_MODERATE_MAX_PPM || voc > UI_TVOC_MODERATE_MAX_PPB) {
            air = "Poor";
            sub = "Ventilation recommended";
        } else if (co2 > UI_ECO2_GOOD_MAX_PPM || voc > UI_TVOC_GOOD_MAX_PPB) {
            air = "Moderate";
            sub = "Air parameters slightly elevated";
        } else {
            air = "Good";
            sub = "Based on eCO₂ and TVOC";
        }
    } else if (!offline) {
        air = "Unavailable";
        sub = lora_node_status.bits.rs485_fault ? "Sensor bus fault" : "Sensor telemetry missing";
    }
    update_label_if_changed(s_home_air_status, air);
    if (s_home_air_sub_lbl != NULL) {
        update_label_if_changed(s_home_air_sub_lbl, sub);
    }
    if (s_home_air_badge != NULL && s_home_air_status != NULL) {
        if (strcmp(air, "Good") == 0) {
            lv_point_t st_size;
            lv_txt_get_size(&st_size, air, &ui_font_48, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
            set_pos_if_changed(s_home_air_badge, 20 + st_size.x + 12, 53);
            set_flag_if_changed(s_home_air_badge, LV_OBJ_FLAG_HIDDEN, false);
        } else {
            set_flag_if_changed(s_home_air_badge, LV_OBJ_FLAG_HIDDEN, true);
        }
    }

    /* Update CO2 */
    char co2_val_tmp[16];
    if (co2_avail) {
        uint16_t ppm = lora_node_status.bits.co2_ppm;
        snprintf(co2_val_tmp, sizeof(co2_val_tmp), "%u", (unsigned)ppm);
        ui_quality_category_t q = (ppm <= UI_ECO2_GOOD_MAX_PPM) ? UI_QUALITY_GOOD :
                                  ((ppm <= UI_ECO2_MODERATE_MAX_PPM) ? UI_QUALITY_MODERATE : UI_QUALITY_POOR);
        set_quality_label(NULL, q, s_co2_badge_buf, sizeof(s_co2_badge_buf));
    } else {
        snprintf(co2_val_tmp, sizeof(co2_val_tmp), "—");
        set_quality_label(NULL, UI_QUALITY_UNKNOWN, s_co2_badge_buf, sizeof(s_co2_badge_buf));
    }
    size_t co2_len = strlen(co2_val_tmp);
    const lv_font_t *co2_font = (co2_len > 4) ? &ui_font_24 : &ui_font_34;
    lv_coord_t co2_y = (co2_len > 4) ? 32 : 26;
    lv_coord_t co2_unit_y = (co2_len > 4) ? 38 : 43;
    if (s_home_co2_val != NULL) {
        set_font_if_changed(s_home_co2_val, co2_font, 0);
        set_pos_if_changed(s_home_co2_val, 0, co2_y);
    }
    set_buffer_and_label_if_changed(s_home_co2_val, s_co2_val_buf, sizeof(s_co2_val_buf), co2_val_tmp);
    if (s_home_co2_val != NULL && s_home_co2_unit_lbl != NULL) {
        lv_point_t sz;
        lv_txt_get_size(&sz, s_co2_val_buf, co2_font, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
        set_pos_if_changed(s_home_co2_unit_lbl, sz.x + 6, co2_unit_y);
    }

    /* Update VOC */
    char voc_val_tmp[16];
    if (voc_avail) {
        uint16_t voc = lora_node_status.bits.tvoc_ppb;
        snprintf(voc_val_tmp, sizeof(voc_val_tmp), "%u", (unsigned)voc);
        ui_quality_category_t q = (voc <= UI_TVOC_GOOD_MAX_PPB) ? UI_QUALITY_GOOD :
                                  ((voc <= UI_TVOC_MODERATE_MAX_PPB) ? UI_QUALITY_MODERATE : UI_QUALITY_POOR);
        set_quality_label(NULL, q, s_voc_badge_buf, sizeof(s_voc_badge_buf));
    } else {
        snprintf(voc_val_tmp, sizeof(voc_val_tmp), "—");
        set_quality_label(NULL, UI_QUALITY_UNKNOWN, s_voc_badge_buf, sizeof(s_voc_badge_buf));
    }
    /* Same sizing rule as eCO₂: the unit follows the number, which shrinks above 4 digits */
    size_t voc_len = strlen(voc_val_tmp);
    const lv_font_t *voc_font = (voc_len > 4) ? &ui_font_24 : &ui_font_34;
    lv_coord_t voc_y = (voc_len > 4) ? 32 : 26;
    lv_coord_t voc_unit_y = (voc_len > 4) ? 38 : 43;
    if (s_home_voc_val != NULL) {
        set_font_if_changed(s_home_voc_val, voc_font, 0);
        set_pos_if_changed(s_home_voc_val, 0, voc_y);
    }
    set_buffer_and_label_if_changed(s_home_voc_val, s_voc_val_buf, sizeof(s_voc_val_buf), voc_val_tmp);
    if (s_home_voc_val != NULL && s_home_voc_unit_lbl != NULL) {
        lv_point_t sz;
        lv_txt_get_size(&sz, s_voc_val_buf, voc_font, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
        set_pos_if_changed(s_home_voc_unit_lbl, sz.x + 6, voc_unit_y);
    }

    /* Update Temperature (L2 Balanced Two-line) */
    char temp_val_tmp[24];
    char temp_note_tmp[32];
    bool temp_valid = temp_avail;
    format_metric_number(temp_val_tmp, sizeof(temp_val_tmp), UI_METRIC_TEMP, lora_node_status.bits.temp_deci_c, temp_valid);
    /* Preserve full signed wire-range readings without crossing the 18px floor.
     * Normal room values use 34px; unusually wide values use 24px so number + unit fit in card. */
    lv_point_t temp_size;
    lv_txt_get_size(&temp_size, temp_val_tmp, &ui_font_34, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    const lv_font_t *temp_font = temp_size.x > 105 ? &ui_font_24 : &ui_font_34;
    if (lv_obj_get_style_text_font(s_home_temp_val, 0) != temp_font)
        lv_obj_set_style_text_font(s_home_temp_val, temp_font, 0);
    if (temp_valid) {
        temp_note_tmp[0] = '\0';
        set_flag_if_changed(s_home_temp_note, LV_OBJ_FLAG_HIDDEN, true);
        set_flag_if_changed(s_home_temp_unit_lbl, LV_OBJ_FLAG_HIDDEN, false);
    } else {
        temp_note_tmp[0] = '\0';
        set_flag_if_changed(s_home_temp_note, LV_OBJ_FLAG_HIDDEN, true);
        set_flag_if_changed(s_home_temp_unit_lbl, LV_OBJ_FLAG_HIDDEN, true);
    }
    set_buffer_and_label_if_changed(s_home_temp_val, s_temp_val_buf, sizeof(s_temp_val_buf), temp_val_tmp);
    set_buffer_and_label_if_changed(s_home_temp_note, s_temp_note_buf, sizeof(s_temp_note_buf), temp_note_tmp);
    if (s_home_temp_val != NULL && s_home_temp_unit_lbl != NULL && temp_valid) {
        lv_point_t cur_sz;
        lv_txt_get_size(&cur_sz, s_temp_val_buf, temp_font, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
        lv_coord_t unit_y = (temp_font == &ui_font_24) ? 46 : 57;
        set_pos_if_changed(s_home_temp_unit_lbl, 60 + cur_sz.x + 3, unit_y);
    }

    /* Update Humidity (L2 Balanced Two-line) */
    char humid_val_tmp[24];
    char humid_note_tmp[32];
    bool humid_valid = humid_avail;
    format_metric_number(humid_val_tmp, sizeof(humid_val_tmp), UI_METRIC_HUMIDITY, lora_node_status.bits.humidity_pct, humid_valid);
    if (humid_valid) {
        humid_note_tmp[0] = '\0';
        set_flag_if_changed(s_home_humid_note, LV_OBJ_FLAG_HIDDEN, true);
        set_flag_if_changed(s_home_humid_unit_lbl, LV_OBJ_FLAG_HIDDEN, false);
    } else {
        humid_note_tmp[0] = '\0';
        set_flag_if_changed(s_home_humid_note, LV_OBJ_FLAG_HIDDEN, true);
        set_flag_if_changed(s_home_humid_unit_lbl, LV_OBJ_FLAG_HIDDEN, true);
    }
    set_buffer_and_label_if_changed(s_home_humid_val, s_humid_val_buf, sizeof(s_humid_val_buf), humid_val_tmp);
    set_buffer_and_label_if_changed(s_home_humid_note, s_humid_note_buf, sizeof(s_humid_note_buf), humid_note_tmp);
    if (s_home_humid_val != NULL && s_home_humid_unit_lbl != NULL && humid_valid) {
        lv_point_t cur_sz;
        lv_txt_get_size(&cur_sz, s_humid_val_buf, &ui_font_34, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
        set_pos_if_changed(s_home_humid_unit_lbl, 60 + cur_sz.x + 3, 57);
    }

    /* Update 4 Device Tiles */
    for (uint8_t i = 0; i < UI_DEVICE_COUNT; i++) {
        const ui_hub_device_config_t *cfg = &s_dev_configs[i];
        ui_device_cmd_state_t *cmd = &s_device_cmds[i];

        if (s_home_dev_icons[i] != NULL) {
            set_img_src_if_changed(s_home_dev_icons[i], get_preset_icon_32(cfg->preset));
        }

        set_buffer_and_label_if_changed(s_home_dev_names[i], s_dev_name_buf[i], sizeof(s_dev_name_buf[i]), get_device_display_name(cfg));

        const char *mode_str = get_device_mode_display_string(i, offline);
        set_buffer_and_label_if_changed(s_home_dev_modes[i], s_dev_mode_buf[i], sizeof(s_dev_mode_buf[i]), mode_str);

        uint8_t reported_gpio = lora_node_status_get_relay_gpio(&lora_node_status, i);
        bool reported_on = (reported_gpio ^ cfg->active_low) != 0;

        bool is_error = (cmd->phase == UI_CMD_PHASE_ERROR);
        bool is_pending = (cmd->phase == UI_CMD_PHASE_PENDING);
        bool is_latched = (!offline && !is_error && !is_pending && cfg->mode == UI_MODE_AUTO && s_auto_states[i].paused_error);

        const char *new_state_str = "";
        lv_color_t new_state_color = UI_COLOR_RED;
        bool state_hidden = true;
        bool dash_hidden = true;
        bool switch_hidden = true;
        bool switch_disabled = false;
        bool switch_checked = reported_on;
        bool retry_hidden = true;

        if (offline) {
            dash_hidden = false;
        } else if (is_error) {
            new_state_str = "—";
            new_state_color = UI_COLOR_MUTED;
            state_hidden = false;
            retry_hidden = false;
        } else if (is_pending) {
            switch_hidden = false;
            switch_disabled = true;
        } else {
            switch_hidden = false;
            if (is_latched) retry_hidden = false;
        }
        set_pos_if_changed(s_home_dev_switches[i], is_latched ? 180 : 240, 18);
        set_pos_if_changed(s_home_dev_retries[i], 244, 11);
        set_pos_if_changed(s_home_dev_dashes[i], 250, 18);
        lv_obj_set_width(s_home_dev_names[i], is_latched ? 120 : 176);

        set_buffer_and_label_if_changed(s_home_dev_states[i], s_dev_state_buf[i], sizeof(s_dev_state_buf[i]), new_state_str);
        set_text_color_if_changed(s_home_dev_states[i], new_state_color);
        set_flag_if_changed(s_home_dev_states[i], LV_OBJ_FLAG_HIDDEN, state_hidden);
        set_pos_if_changed(s_home_dev_states[i], 168, 36);

        bool auto_eligible = (!offline && cfg->mode == UI_MODE_AUTO && !s_auto_states[i].paused_error && !s_auto_states[i].suspended_sensor);
        if (s_home_dev_auto_icons[i] != NULL) {
            set_flag_if_changed(s_home_dev_auto_icons[i], LV_OBJ_FLAG_HIDDEN, !auto_eligible);
        }
        set_pos_if_changed(s_home_dev_modes[i], auto_eligible ? 74 : 52, 36);
        lv_obj_set_width(s_home_dev_modes[i], is_latched ? 120 : (auto_eligible ? (state_hidden ? 150 : 80) : (state_hidden ? 176 : 106)));

        set_flag_if_changed(s_home_dev_dashes[i], LV_OBJ_FLAG_HIDDEN, dash_hidden);
        set_flag_if_changed(s_home_dev_retries[i], LV_OBJ_FLAG_HIDDEN, retry_hidden);

        set_flag_if_changed(s_home_dev_switches[i], LV_OBJ_FLAG_HIDDEN, switch_hidden);
        set_state_if_changed(s_home_dev_switches[i], LV_STATE_DISABLED, switch_disabled);
        set_state_if_changed(s_home_dev_switches[i], LV_STATE_CHECKED, switch_checked);
    }
}

/* =========================================================================
 * TRENDS PAGE (P1.8 Clock wrap)
 * ========================================================================= */
/* Quiet time after the last metric tap before the card and chart are redrawn. */
#ifndef UI_TRENDS_SETTLE_MS
#define UI_TRENDS_SETTLE_MS 150u
#endif

static void update_trends_title(ui_metric_id_t metric)
{
    static const char *titles[4] = { "eCO₂", "TVOC", "Temperature", "Humidity" };
    if (s_trends_title_lbl != NULL && metric < UI_METRIC_COUNT) {
        update_label_if_changed(s_trends_title_lbl, titles[metric]);
    }
}

static void update_trends_tabs(void)
{
    ui_metric_id_t active_tab = s_pending_trend_chart ? s_pending_trend_metric : s_selected_metric;
    for (uintptr_t i = 0; i < 4; i++) {
        if (s_trends_tab_cards[i] == NULL) continue;
        bool sel = (i == (uintptr_t)active_tab);
        bool is_checked = lv_obj_has_state(s_trends_tab_cards[i], LV_STATE_CHECKED);
        if (sel != is_checked) {
            if (sel) {
                lv_obj_add_state(s_trends_tab_cards[i], LV_STATE_CHECKED);
                if (s_trends_tab_icons[i]) lv_obj_set_style_img_recolor(s_trends_tab_icons[i], lv_color_white(), 0);
                if (s_trends_tab_names[i]) set_text_color_if_changed(s_trends_tab_names[i], lv_color_white());
                if (s_trends_tab_vals[i]) set_text_color_if_changed(s_trends_tab_vals[i], lv_color_white());
                if (s_trends_tab_units[i]) set_text_color_if_changed(s_trends_tab_units[i], lv_color_white());
                if (s_trends_tab_dots[i]) set_flag_if_changed(s_trends_tab_dots[i], LV_OBJ_FLAG_HIDDEN, false);
            } else {
                lv_obj_clear_state(s_trends_tab_cards[i], LV_STATE_CHECKED);
                if (s_trends_tab_icons[i]) lv_obj_set_style_img_recolor(s_trends_tab_icons[i], UI_COLOR_INK, 0);
                if (s_trends_tab_names[i]) set_text_color_if_changed(s_trends_tab_names[i], UI_COLOR_INK);
                if (s_trends_tab_vals[i]) set_text_color_if_changed(s_trends_tab_vals[i], UI_COLOR_INK);
                if (s_trends_tab_units[i]) set_text_color_if_changed(s_trends_tab_units[i], UI_COLOR_MUTED);
                if (s_trends_tab_dots[i]) set_flag_if_changed(s_trends_tab_dots[i], LV_OBJ_FLAG_HIDDEN, true);
            }
        }
    }
}

static bool ui_is_disp_busy_or_dirty(void)
{
    lv_disp_t *disp = lv_disp_get_default();
    if (!disp) return false;
    if (disp->inv_p > 0) return true;
    if (disp->rendering_in_progress) return true;
    if (disp->driver && disp->driver->draw_buf && disp->driver->draw_buf->flushing) return true;
    return false;
}

static void on_metric_tab_click(lv_event_t *e)
{
    uintptr_t metric = (uintptr_t)lv_event_get_user_data(e);
    if (metric >= UI_METRIC_COUNT) return;
    ui_metric_id_t m = (ui_metric_id_t)metric;
    if (s_current_page == UI_PAGE_TRENDS && s_selected_metric == m && !s_pending_trend_chart) {
        /* Already selected on Trends page and chart not pending: strictly a visual & memory no-op (R1) */
        return;
    }

    /* Stage 1: highlight on touch-down. The tab has no separate pressed look, so
     * one tap repaints the two affected tabs once instead of press + release. */
    s_pending_trend_metric = m;
    s_pending_trend_chart = true;
    s_stage1_flush_pending = true;
    s_pending_trend_tick = lv_tick_get();
    if (s_current_page == UI_PAGE_TRENDS && s_page_container != NULL) {
        update_trends_tabs();
        lv_disp_t *disp = lv_disp_get_default();
        if (disp && disp->refr_timer) {
            lv_timer_ready(disp->refr_timer);
        }
    }
}

static inline int32_t clamp_to_i32(int64_t v)
{
    if (v < (int64_t)INT32_MIN) return INT32_MIN;
    if (v > (int64_t)INT32_MAX) return INT32_MAX;
    return (int32_t)v;
}

static int64_t round_down_mult(int64_t val, int64_t step)
{
    if (step <= 0) return val;
    if (val >= 0) {
        return (val / step) * step;
    } else {
        return -(((-val + step - 1) / step) * step);
    }
}

/* Round-number Y ticks (B4): exactly three labels lo, lo+s, lo+2s */
static void trends_calc_y_axis(int32_t min_v, int32_t max_v, ui_metric_id_t metric,
                               int64_t *out_lo, int64_t *out_step)
{
    static const int64_t temp_steps[] = {
        1, 2, 5, 10, 20, 25, 50, 100, 200, 250, 500, 1000,
        2000, 2500, 5000, 10000, 20000, 25000, 50000, 100000,
        200000, 250000, 500000, 1000000, 2000000, 2500000, 5000000,
        10000000, 20000000, 25000000, 50000000, 100000000, 200000000,
        250000000, 500000000, 1000000000, 2000000000, 2500000000LL
    };
    static const int64_t general_steps[] = {
        1, 2, 5, 10, 20, 25, 50, 100, 200, 250, 500,
        1000, 2000, 2500, 5000, 10000, 20000, 25000, 50000, 100000,
        200000, 250000, 500000, 1000000, 2000000, 2500000, 5000000,
        10000000, 20000000, 25000000, 50000000, 100000000, 200000000,
        250000000, 500000000, 1000000000, 2000000000, 2500000000LL
    };

    int64_t vmin = min_v;
    int64_t vmax = max_v;
    int64_t span = vmax - vmin;
    int64_t min_span = (metric == UI_METRIC_TEMP) ? 10 : 20;
    if (span < min_span) {
        int64_t mid = (vmin + vmax) / 2;
        int64_t half = min_span / 2;
        vmin = mid - half;
        vmax = mid + (min_span - half);
        span = min_span;
    }
    int64_t pad = span * 5 / 100;
    if (pad < 1) pad = 1;
    int64_t pmin = vmin - pad;
    int64_t pmax = vmax + pad;

    const int64_t *steps = (metric == UI_METRIC_TEMP) ? temp_steps : general_steps;
    size_t num_steps = (metric == UI_METRIC_TEMP) ? (sizeof(temp_steps) / sizeof(temp_steps[0]))
                                                  : (sizeof(general_steps) / sizeof(general_steps[0]));

    int64_t chosen_step = steps[num_steps - 1];
    int64_t chosen_lo = round_down_mult(pmin, chosen_step);

    for (size_t i = 0; i < num_steps; i++) {
        int64_t s = steps[i];
        int64_t lo = round_down_mult(pmin, s);
        if (lo + 2 * s >= pmax) {
            chosen_step = s;
            chosen_lo = lo;
            break;
        }
    }
    *out_lo = chosen_lo;
    *out_step = chosen_step;
}

static lv_coord_t trends_plot_value(int32_t value, bool valid, int32_t range_min, int32_t range_max)
{
    if (!valid) return LV_CHART_POINT_NONE;
    if (range_max <= range_min) return 500;
    if (value <= range_min) return 0;
    if (value >= range_max) return 1000;
    int64_t span = (int64_t)range_max - range_min;
    return (lv_coord_t)((((int64_t)value - range_min) * 1000 + span / 2) / span);
}

static void build_trends_page(void)
{
    lv_obj_t *p = s_page_container;
    s_history_rendered_rev[s_selected_metric] = 0xFFFFFFFF;
    s_last_rendered_metric = (ui_metric_id_t)0xFF;

    /* Four full-height selectors on the left rail (w: 184, h: 80 each, at y: i * 88) */
    static const char *metric_names[4] = { "eCO₂", "TVOC", "Temperature", "Humidity" };
    static const lv_img_dsc_t *metric_icons[4] = {
        &ui_icon_leaf_24,
        &ui_icon_air_28,
        &ui_icon_thermometer_24,
        &ui_icon_drop_24
    };
    for (uintptr_t i = 0; i < 4; i++) {
        lv_obj_t *btn = lv_btn_create(p);
        lv_obj_remove_style_all(btn);
        lv_obj_set_pos(btn, 0, (lv_coord_t)(i * 88));
        lv_obj_set_size(btn, 184, 80);
        bool sel = (i == (uintptr_t)s_selected_metric);
        lv_obj_add_style(btn, &ui_style_card_porcelain, LV_STATE_DEFAULT);
        lv_obj_add_style(btn, &ui_style_card_hero, LV_STATE_CHECKED);
        if (sel) lv_obj_add_state(btn, LV_STATE_CHECKED);
        else lv_obj_clear_state(btn, LV_STATE_CHECKED);
        lv_obj_add_event_cb(btn, on_metric_tab_click, LV_EVENT_PRESSED, (void *)i);
        s_trends_tab_btns[i] = btn;
        s_trends_tab_cards[i] = btn;

        /* Sensor icon (24px) */
        s_trends_tab_icons[i] = add_icon(btn, metric_icons[i], sel ? lv_color_white() : UI_COLOR_INK);
        lv_obj_set_pos(s_trends_tab_icons[i], 12, 12);

        /* Sensor name */
        s_trends_tab_names[i] = add_static_label(btn, metric_names[i], &ui_font_18,
                                                 sel ? lv_color_white() : UI_COLOR_INK);
        lv_obj_set_pos(s_trends_tab_names[i], 42, 12);

        /* Celadon dot */
        s_trends_tab_dots[i] = lv_obj_create(btn);
        lv_obj_remove_style_all(s_trends_tab_dots[i]);
        lv_obj_set_pos(s_trends_tab_dots[i], 160, 14);
        lv_obj_set_size(s_trends_tab_dots[i], 8, 8);
        lv_obj_set_style_radius(s_trends_tab_dots[i], 4, 0);
        lv_obj_set_style_bg_color(s_trends_tab_dots[i], UI_COLOR_CELADON, 0);
        lv_obj_set_style_bg_opa(s_trends_tab_dots[i], LV_OPA_COVER, 0);
        if (!sel) lv_obj_add_flag(s_trends_tab_dots[i], LV_OBJ_FLAG_HIDDEN);

        /* Value & Unit in selector */
        s_trends_tab_vals[i] = add_static_label(btn, s_trends_tab_val_buf[i], &ui_font_24,
                                                sel ? lv_color_white() : UI_COLOR_INK);
        lv_obj_set_pos(s_trends_tab_vals[i], 42, 39);

        s_trends_tab_units[i] = add_static_label(btn, s_trends_tab_unit_buf[i], &ui_font_18,
                                                 sel ? lv_color_white() : UI_COLOR_MUTED);
        lv_obj_set_pos(s_trends_tab_units[i], 42 + 50, 45);
    }

    /* Chart Card on the right (w: 552, h: 344 at x: 200, y: 0) */
    lv_obj_t *card = lv_obj_create(p);
    lv_obj_remove_style_all(card);
    lv_obj_set_pos(card, 200, 0);
    lv_obj_set_size(card, 552, 344);
    lv_obj_add_style(card, &ui_style_card_porcelain, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    /* Title row: metric name on the left at (20, 14), status badge in fixed top-right slot */
    static const char *titles[4] = { "eCO₂", "TVOC", "Temperature", "Humidity" };
    s_trends_title_lbl = add_static_label(card, titles[s_selected_metric], &ui_font_34, UI_COLOR_INK);
    lv_obj_set_pos(s_trends_title_lbl, 20, 14);

    /* Data unavailable badge */
    s_trends_unavail_badge = lv_obj_create(card);
    lv_obj_remove_style_all(s_trends_unavail_badge);
    lv_obj_align(s_trends_unavail_badge, LV_ALIGN_TOP_RIGHT, -20, 23);
    lv_obj_set_size(s_trends_unavail_badge, LV_SIZE_CONTENT, 26);
    lv_obj_set_style_bg_color(s_trends_unavail_badge, UI_COLOR_RED_SOFT, 0);
    lv_obj_set_style_bg_opa(s_trends_unavail_badge, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(s_trends_unavail_badge, 6, 0);
    lv_obj_set_style_pad_hor(s_trends_unavail_badge, 8, 0);
    lv_obj_set_style_pad_ver(s_trends_unavail_badge, 3, 0);
    add_static_label(s_trends_unavail_badge, "Data unavailable", &ui_font_18, UI_COLOR_RED);

    /* Y-axis column (x 12..68): unit on top at y=96, right-aligned like the tick labels below */
    s_trends_unit_lbl = add_static_label(card, s_trends_unit_buf, &ui_font_18, UI_COLOR_MUTED);
    lv_obj_set_width(s_trends_unit_lbl, 56);
    lv_obj_set_style_text_align(s_trends_unit_lbl, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(s_trends_unit_lbl, 12, 96);

    /* Empty state: no readings, or a single reading that cannot form a trend yet */
    s_trends_empty_box = lv_obj_create(card);
    lv_obj_remove_style_all(s_trends_empty_box);
    lv_obj_set_pos(s_trends_empty_box, 20, 108);
    lv_obj_set_size(s_trends_empty_box, 512, 191);
    lv_obj_set_flex_flow(s_trends_empty_box, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_trends_empty_box, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(s_trends_empty_box, LV_OBJ_FLAG_HIDDEN);
    add_icon(s_trends_empty_box, &ui_icon_chart_24, UI_COLOR_MUTED);
    s_trends_empty_lbl = add_static_label(s_trends_empty_box, "No readings yet", &ui_font_24, UI_COLOR_INK);
    s_trends_empty_hint = add_static_label(s_trends_empty_box, "The trend appears after the next reading",
                                           &ui_font_18, UI_COLOR_MUTED);
    lv_obj_set_style_pad_top(s_trends_empty_hint, 6, 0);
    lv_obj_add_flag(s_trends_empty_hint, LV_OBJ_FLAG_HIDDEN);

    /* 3 right-aligned Y-axis labels, each centred on a grid line (y 147 / 202 / 257 -> lbl y 136 / 191 / 246) */
    static const lv_coord_t y_lbl_ys[3] = { 136, 191, 246 };
    for (int i = 0; i < 3; i++) {
        s_trends_y_lbls[i] = add_static_label(card, s_trends_y_buf[i], &ui_font_18, UI_COLOR_MUTED);
        lv_obj_set_width(s_trends_y_lbls[i], 56);
        lv_obj_set_style_text_align(s_trends_y_lbls[i], LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_set_pos(s_trends_y_lbls[i], 12, y_lbl_ys[i]);
    }

    /* Chart at x: 72, y: 136, w: 460, h: 132.
     * Explicit padding keeps the plot area at x 83..521, y 147..257 so grid lines and axis
     * labels stay aligned. */
    s_trends_chart = lv_chart_create(card);
    lv_obj_set_pos(s_trends_chart, 72, 136);
    lv_obj_set_size(s_trends_chart, 460, 132);
    lv_obj_set_style_pad_all(s_trends_chart, 11, 0);
    lv_obj_set_style_bg_color(s_trends_chart, UI_COLOR_SURFACE, 0);
    lv_obj_set_style_bg_opa(s_trends_chart, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_trends_chart, 0, 0);
    lv_obj_set_style_line_color(s_trends_chart, UI_COLOR_DIVIDER, LV_PART_MAIN);
    lv_chart_set_type(s_trends_chart, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(s_trends_chart, UI_HISTORY_CAPACITY);
    lv_chart_set_div_line_count(s_trends_chart, 3, 0);
    s_trends_chart_series = lv_chart_add_series(s_trends_chart, UI_COLOR_FOREST, LV_CHART_AXIS_PRIMARY_Y);
    lv_chart_set_ext_y_array(s_trends_chart, s_trends_chart_series, s_chart_ext_y);
    /* Native line/markers only: no polygon blending or point-inspection events. */
    lv_chart_set_range(s_trends_chart, LV_CHART_AXIS_PRIMARY_Y, 0, 1000);
    lv_obj_set_style_line_width(s_trends_chart, 2, LV_PART_ITEMS);
    lv_obj_set_style_line_opa(s_trends_chart, LV_OPA_COVER, LV_PART_ITEMS);
    lv_obj_set_style_size(s_trends_chart, 4, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(s_trends_chart, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_border_width(s_trends_chart, 0, LV_PART_INDICATOR);
    lv_obj_clear_flag(s_trends_chart, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

    /* Time Axis row at y=276: labels centred under slots 0, 5, 10 (x 83/229/375 -> box x 43/189/335, w 80);
     * slot 15 right-aligned to chart edge x 532 -> box x 452, w 80. */
    static const lv_coord_t time_xs[4] = { 43, 189, 335, 452 };
    for (int i = 0; i < 4; i++) {
        s_trends_time_labels[i] = add_static_label(card, s_time_axis_buf[i], &ui_font_18, UI_COLOR_MUTED);
        lv_obj_set_width(s_trends_time_labels[i], 80);
        lv_obj_set_style_text_align(s_trends_time_labels[i], (i == 3) ? LV_TEXT_ALIGN_RIGHT : LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_pos(s_trends_time_labels[i], time_xs[i], 276);
    }

    update_trends_page_widgets();
}

static void update_trends_page_widgets(void)
{
    if (s_current_page != UI_PAGE_TRENDS || s_trends_chart == NULL) return;

    update_trends_title(s_selected_metric);

    uint32_t now = lv_tick_get();

    /* Data Unavailable badge on chart card; the live value itself is shown on the selector */
    set_flag_if_changed(s_trends_unavail_badge, LV_OBJ_FLAG_HIDDEN,
                        ui_is_sensor_available(s_selected_metric, now));

    /* Update mini values on the 4 metric selector cards */
    for (int i = 0; i < 4; i++) {
        if (s_trends_tab_vals[i] != NULL) {
            int32_t val = 0;
            bool v_valid = ui_is_sensor_available((ui_metric_id_t)i, now);
            if (v_valid) {
                switch (i) {
                    case 0: val = lora_node_status.bits.co2_ppm; break;
                    case 1: val = lora_node_status.bits.tvoc_ppb; break;
                    case 2: val = lora_node_status.bits.temp_deci_c; break;
                    case 3: val = lora_node_status.bits.humidity_pct; break;
                }
            }
            char tab_num[16];
            format_metric_number(tab_num, sizeof(tab_num), (ui_metric_id_t)i, val, v_valid);
            set_buffer_and_label_if_changed(s_trends_tab_vals[i], s_trends_tab_val_buf[i], sizeof(s_trends_tab_val_buf[i]), tab_num);

            const char *tu = v_valid ? get_metric_unit_str((ui_metric_id_t)i) : "";
            if (s_trends_tab_units[i] != NULL) {
                set_buffer_and_label_if_changed(s_trends_tab_units[i], s_trends_tab_unit_buf[i], sizeof(s_trends_tab_unit_buf[i]), tu);
                lv_point_t tsz;
                lv_txt_get_size(&tsz, s_trends_tab_val_buf[i], &ui_font_24, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
                set_pos_if_changed(s_trends_tab_units[i], 42 + tsz.x + 4, 45);
            }
        }
    }

    bool hist_changed = (s_history_rendered_rev[s_selected_metric] != s_history_rev[s_selected_metric] ||
                         s_selected_metric != s_last_rendered_metric);

    if (hist_changed) {
        /* Min/Max over valid samples: only used to scale the plot and the axis labels */
        int32_t min_v = INT32_MAX;
        int32_t max_v = INT32_MIN;
        uint16_t valid_cnt = 0;

        for (uint16_t i = 0; i < s_history_count[s_selected_metric]; i++) {
            if (s_history[s_selected_metric][i].valid) {
                int32_t v = s_history[s_selected_metric][i].value;
                if (v < min_v) min_v = v;
                if (v > max_v) max_v = v;
                valid_cnt++;
            }
        }

        /* Unit above chart */
        if (s_trends_unit_lbl != NULL) {
            const char *top_unit = get_metric_unit_str(s_selected_metric);
            set_buffer_and_label_if_changed(s_trends_unit_lbl, s_trends_unit_buf, sizeof(s_trends_unit_buf), top_unit);
        }

        /* Plot area or Empty State. One reading cannot show a trend, so it gets a message
         * instead of a lone marker. */
        if (valid_cnt < 2) {
            update_label_if_changed(s_trends_empty_lbl, (valid_cnt == 0) ? "No readings yet" : "Collecting readings…");
            set_flag_if_changed(s_trends_empty_hint, LV_OBJ_FLAG_HIDDEN, valid_cnt == 0);
            if (s_trends_unit_lbl) set_flag_if_changed(s_trends_unit_lbl, LV_OBJ_FLAG_HIDDEN, true);
            set_flag_if_changed(s_trends_empty_box, LV_OBJ_FLAG_HIDDEN, false);
            set_flag_if_changed(s_trends_chart, LV_OBJ_FLAG_HIDDEN, true);
            for (int i = 0; i < 3; i++) {
                if (s_trends_y_lbls[i]) set_flag_if_changed(s_trends_y_lbls[i], LV_OBJ_FLAG_HIDDEN, true);
            }
            for (int i = 0; i < 4; i++) {
                if (s_trends_time_labels[i]) set_flag_if_changed(s_trends_time_labels[i], LV_OBJ_FLAG_HIDDEN, true);
            }
        } else {
            set_flag_if_changed(s_trends_empty_box, LV_OBJ_FLAG_HIDDEN, true);
            set_flag_if_changed(s_trends_chart, LV_OBJ_FLAG_HIDDEN, false);
            if (s_trends_unit_lbl) set_flag_if_changed(s_trends_unit_lbl, LV_OBJ_FLAG_HIDDEN, false);

            /* Chart always has UI_HISTORY_CAPACITY (16) points */
            if (lv_chart_get_point_count(s_trends_chart) != UI_HISTORY_CAPACITY) {
                lv_chart_set_point_count(s_trends_chart, UI_HISTORY_CAPACITY);
            }

            /* Time Axis: Fixed 15-minute window */
            if (!s_history_externally_fed[s_selected_metric]) {
                /* Autonomous: fixed static strings */
                set_buffer_and_label_if_changed(s_trends_time_labels[0], s_time_axis_buf[0], sizeof(s_time_axis_buf[0]), "-15 min");
                set_buffer_and_label_if_changed(s_trends_time_labels[1], s_time_axis_buf[1], sizeof(s_time_axis_buf[1]), "-10 min");
                set_buffer_and_label_if_changed(s_trends_time_labels[2], s_time_axis_buf[2], sizeof(s_time_axis_buf[2]), "-5 min");
                set_buffer_and_label_if_changed(s_trends_time_labels[3], s_time_axis_buf[3], sizeof(s_time_axis_buf[3]), "Now");
                for (int i = 0; i < 4; i++) {
                    if (s_trends_time_labels[i]) set_flag_if_changed(s_trends_time_labels[i], LV_OBJ_FLAG_HIDDEN, false);
                }
            } else {
                /* External feed: wall-clock timestamps ending at newest sample minute */
                uint16_t cnt = s_history_count[s_selected_metric];
                uint32_t newest_m = (cnt > 0) ? s_history[s_selected_metric][cnt - 1].minute_of_day : 0u;
                static const uint16_t slots[4] = { 0, 5, 10, 15 };
                for (int i = 0; i < 4; i++) {
                    uint32_t m = (newest_m + 1440u - (15u - slots[i])) % 1440u;
                    char time_tmp[16];
                    snprintf(time_tmp, sizeof(time_tmp), "%02u:%02u", (unsigned)(m / 60u), (unsigned)(m % 60u));
                    set_buffer_and_label_if_changed(s_trends_time_labels[i], s_time_axis_buf[i], sizeof(s_time_axis_buf[i]), time_tmp);
                    set_flag_if_changed(s_trends_time_labels[i], LV_OBJ_FLAG_HIDDEN, false);
                }
            }

            /* Calculate round-number Y ticks */
            int64_t lo = 0, step = 1;
            trends_calc_y_axis(min_v, max_v, s_selected_metric, &lo, &step);
            int64_t val_bottom = lo;
            int64_t val_mid = lo + step;
            int64_t val_top = lo + 2 * step;

            int32_t range_min = clamp_to_i32(val_bottom);
            int32_t range_mid = clamp_to_i32(val_mid);
            int32_t range_max = clamp_to_i32(val_top);

            /* 3 Y-axis labels: top (lo + 2*step), mid (lo + step), bottom (lo) */
            char y_tmp[3][16];
            if (s_selected_metric == UI_METRIC_TEMP) {
                format_temp_c(y_tmp[0], sizeof(y_tmp[0]), range_max);
                format_temp_c(y_tmp[1], sizeof(y_tmp[1]), range_mid);
                format_temp_c(y_tmp[2], sizeof(y_tmp[2]), range_min);
            } else {
                snprintf(y_tmp[0], sizeof(y_tmp[0]), "%ld", (long)range_max);
                snprintf(y_tmp[1], sizeof(y_tmp[1]), "%ld", (long)range_mid);
                snprintf(y_tmp[2], sizeof(y_tmp[2]), "%ld", (long)range_min);
            }
            for (int i = 0; i < 3; i++) {
                if (s_trends_y_lbls[i]) {
                    set_buffer_and_label_if_changed(s_trends_y_lbls[i], s_trends_y_buf[i], sizeof(s_trends_y_buf[i]), y_tmp[i]);
                    set_flag_if_changed(s_trends_y_lbls[i], LV_OBJ_FLAG_HIDDEN, false);
                }
            }

            /* Fill 16 chart slots: newest sample is slot 15; count < 16 has leading LV_CHART_POINT_NONE */
            uint16_t count = s_history_count[s_selected_metric];
            if (count > UI_HISTORY_CAPACITY) count = UI_HISTORY_CAPACITY;
            uint16_t offset = UI_HISTORY_CAPACITY - count;
            bool points_changed = false;

            for (uint16_t slot = 0; slot < UI_HISTORY_CAPACITY; slot++) {
                lv_coord_t new_y = LV_CHART_POINT_NONE;
                if (slot >= offset) {
                    uint16_t h_idx = slot - offset;
                    bool valid = s_history[s_selected_metric][h_idx].valid;
                    new_y = trends_plot_value(valid ? s_history[s_selected_metric][h_idx].value : 0,
                                              valid, range_min, range_max);
                }
                if (s_chart_ext_y[slot] != new_y) {
                    s_chart_ext_y[slot] = new_y;
                    points_changed = true;
                }
            }
            if (points_changed) {
                lv_chart_refresh(s_trends_chart);
            }
        }

        s_history_rendered_rev[s_selected_metric] = s_history_rev[s_selected_metric];
        s_last_rendered_metric = s_selected_metric;
    }
}

/* =========================================================================
 * DEVICES PAGE (P2.18 No Card Overflow, P2.19 Two-line Name)
 * ========================================================================= */
static void on_device_configure_click(lv_event_t *e)
{
    uint8_t idx = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
    open_device_dialog(idx);
}

static void build_devices_page(void)
{
    lv_obj_t *p = s_page_container;

    /* 2x2 Grid of cards (368x164 each at x: col*384, y: row*180) */
    for (uint8_t i = 0; i < UI_DEVICE_COUNT; i++) {
        lv_coord_t col = (i % 2);
        lv_coord_t row = (i / 2);
        lv_coord_t x = col * 384;
        lv_coord_t y = row * 180;

        lv_obj_t *card = lv_obj_create(p);
        lv_obj_remove_style_all(card);
        lv_obj_set_pos(card, x, y);
        lv_obj_set_size(card, 368, 164);
        lv_obj_add_style(card, &ui_style_card_porcelain, 0);
        lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

        /* 42px outline icon */
        s_dev_page_icons[i] = add_icon(card, get_preset_icon_42(s_dev_configs[i].preset), UI_COLOR_INK);
        lv_obj_set_pos(s_dev_page_icons[i], 16, 29);

        /* Name Label (font 24, pos 72, 22, width 210 px) */
        s_dev_page_names[i] = add_static_label(card, s_dev_name_buf[i], &ui_font_24, UI_COLOR_INK);
        lv_obj_set_pos(s_dev_page_names[i], 72, 22);
        lv_obj_set_width(s_dev_page_names[i], 210);
        lv_label_set_long_mode(s_dev_page_names[i], LV_LABEL_LONG_CLIP);

        /* Auto leaf icon */
        s_dev_page_auto_icons[i] = add_icon(card, &ui_icon_leaf_18, UI_COLOR_FOREST);
        lv_obj_set_pos(s_dev_page_auto_icons[i], 72, 56);
        lv_obj_add_flag(s_dev_page_auto_icons[i], LV_OBJ_FLAG_HIDDEN);

        /* Mode & State on second row */
        s_dev_page_modes[i] = add_static_label(card, s_dev_mode_buf[i], &ui_font_18, UI_COLOR_MUTED);
        lv_obj_set_pos(s_dev_page_modes[i], 72, 54);
        lv_obj_set_width(s_dev_page_modes[i], 160);
        lv_label_set_long_mode(s_dev_page_modes[i], LV_LABEL_LONG_CLIP);

        s_dev_page_states[i] = add_static_label(card, s_dev_state_buf[i], &ui_font_18, UI_COLOR_AMBER);
        lv_obj_set_pos(s_dev_page_states[i], 176, 54);

        /* Switch and Retry at top right */
        s_dev_page_dashes[i] = add_static_label(card, "—", &ui_font_24, UI_COLOR_MUTED);
        lv_obj_set_pos(s_dev_page_dashes[i], 298, 31);

        s_dev_page_switches[i] = lv_switch_create(card);
        lv_obj_set_size(s_dev_page_switches[i], 54, 30);
        lv_obj_set_pos(s_dev_page_switches[i], 298, 35);
        lv_obj_set_ext_click_area(s_dev_page_switches[i], 7);
        lv_obj_add_style(s_dev_page_switches[i], &ui_style_switch_main, LV_PART_MAIN);
        lv_obj_add_style(s_dev_page_switches[i], &ui_style_switch_indicator, LV_PART_INDICATOR | LV_STATE_CHECKED);
        lv_obj_add_event_cb(s_dev_page_switches[i], on_device_switch_click, LV_EVENT_VALUE_CHANGED, (void *)(uintptr_t)i);

        s_dev_page_retries[i] = add_static_label(card, "Retry", &ui_font_18, UI_COLOR_BLUE);
        lv_obj_set_pos(s_dev_page_retries[i], 304, 28);
        lv_obj_set_size(s_dev_page_retries[i], 48, 44);
        lv_obj_set_ext_click_area(s_dev_page_retries[i], 0);
        lv_obj_add_flag(s_dev_page_retries[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(s_dev_page_retries[i], on_device_retry_click, LV_EVENT_CLICKED, (void *)(uintptr_t)i);

        /* Divider line at y: 100 */
        lv_obj_t *div = lv_obj_create(card);
        lv_obj_remove_style_all(div);
        lv_obj_set_pos(div, 16, 100);
        lv_obj_set_size(div, 336, 1);
        lv_obj_set_style_bg_color(div, UI_COLOR_DIVIDER, 0);
        lv_obj_set_style_bg_opa(div, LV_OPA_COVER, 0);

        /* Settings action row (clickable, h: 63) */
        lv_obj_t *btn_cfg = lv_btn_create(card);
        lv_obj_remove_style_all(btn_cfg);
        lv_obj_set_pos(btn_cfg, 0, 101);
        lv_obj_set_size(btn_cfg, 368, 63);
        lv_obj_add_style(btn_cfg, &ui_style_pressed, LV_STATE_PRESSED);
        lv_obj_add_event_cb(btn_cfg, on_device_configure_click, LV_EVENT_CLICKED, (void *)(uintptr_t)i);

        lv_obj_t *cfg_ico = add_icon(btn_cfg, &ui_icon_sliders_24, UI_COLOR_INK);
        lv_obj_set_pos(cfg_ico, 25, 20);

        lv_obj_t *cfg_label = add_static_label(btn_cfg, "Settings", &ui_font_18, UI_COLOR_INK);
        lv_obj_set_pos(cfg_label, 72, 20);

        lv_obj_t *cfg_arrow = add_icon(btn_cfg, &ui_icon_chevron_right_20, UI_COLOR_MUTED);
        lv_obj_set_pos(cfg_arrow, 332, 22);
    }

    update_devices_page_widgets();
}

static void update_devices_page_widgets(void)
{
    if (s_current_page != UI_PAGE_DEVICES || s_dev_page_names[0] == NULL) return;

    uint32_t now = lv_tick_get();
    bool offline = !is_link_connected(now);

    for (uint8_t i = 0; i < UI_DEVICE_COUNT; i++) {
        const ui_hub_device_config_t *cfg = &s_dev_configs[i];
        ui_device_cmd_state_t *cmd = &s_device_cmds[i];

        if (s_dev_page_icons[i] != NULL) {
            set_img_src_if_changed(s_dev_page_icons[i], get_preset_icon_42(cfg->preset));
        }

        const char *display_name = get_device_display_name(cfg);
        set_buffer_and_label_if_changed(s_dev_page_names[i], s_dev_name_buf[i], sizeof(s_dev_name_buf[i]), display_name);

        const char *mode_str = get_device_mode_display_string(i, offline);
        set_buffer_and_label_if_changed(s_dev_page_modes[i], s_dev_mode_buf[i], sizeof(s_dev_mode_buf[i]), mode_str);

        uint8_t reported_gpio = lora_node_status_get_relay_gpio(&lora_node_status, i);
        bool reported_on = (reported_gpio ^ cfg->active_low) != 0;

        bool is_error = (cmd->phase == UI_CMD_PHASE_ERROR);
        bool is_pending = (cmd->phase == UI_CMD_PHASE_PENDING);
        bool is_latched = (!offline && !is_error && !is_pending && cfg->mode == UI_MODE_AUTO && s_auto_states[i].paused_error);

        const char *new_state_str = "";
        lv_color_t new_state_color = UI_COLOR_MUTED;
        bool state_hidden = true;
        bool dash_hidden = true;
        bool switch_hidden = true;
        bool switch_disabled = false;
        bool switch_checked = reported_on;
        bool retry_hidden = true;

        lv_obj_set_width(s_dev_page_names[i], is_latched ? 150 : 210);

        if (offline) {
            dash_hidden = false;
        } else if (is_error) {
            new_state_str = "—";
            new_state_color = UI_COLOR_MUTED;
            state_hidden = false;
            retry_hidden = false;
            set_pos_if_changed(s_dev_page_retries[i], 304, 28);
        } else if (is_pending) {
            switch_hidden = false;
            switch_disabled = true;
            switch_checked = reported_on;
            set_pos_if_changed(s_dev_page_switches[i], 298, 35);
        } else if (is_latched) {
            switch_hidden = false;
            switch_disabled = false;
            switch_checked = reported_on;
            retry_hidden = false;
            set_pos_if_changed(s_dev_page_switches[i], 240, 35);
            set_pos_if_changed(s_dev_page_retries[i], 304, 28);
        } else {
            switch_hidden = false;
            switch_disabled = false;
            switch_checked = reported_on;
            set_pos_if_changed(s_dev_page_switches[i], 298, 35);
        }

        set_buffer_and_label_if_changed(s_dev_page_states[i], s_dev_state_buf[i], sizeof(s_dev_state_buf[i]), new_state_str);
        set_text_color_if_changed(s_dev_page_states[i], new_state_color);
        set_flag_if_changed(s_dev_page_states[i], LV_OBJ_FLAG_HIDDEN, state_hidden);

        bool is_auto = (!offline && cfg->mode == UI_MODE_AUTO);
        if (s_dev_page_auto_icons[i] != NULL) {
            set_flag_if_changed(s_dev_page_auto_icons[i], LV_OBJ_FLAG_HIDDEN, !is_auto);
        }

        lv_point_t mode_sz;
        lv_txt_get_size(&mode_sz, mode_str, &ui_font_18, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
        lv_coord_t mode_x = is_auto ? 94 : 72;
        lv_obj_set_width(s_dev_page_modes[i], is_latched ? 136 : 150);
        set_pos_if_changed(s_dev_page_modes[i], mode_x, 54);
        set_pos_if_changed(s_dev_page_states[i], mode_x + mode_sz.x + 14, 54);

        set_flag_if_changed(s_dev_page_dashes[i], LV_OBJ_FLAG_HIDDEN, dash_hidden);
        set_flag_if_changed(s_dev_page_retries[i], LV_OBJ_FLAG_HIDDEN, retry_hidden);

        set_flag_if_changed(s_dev_page_switches[i], LV_OBJ_FLAG_HIDDEN, switch_hidden);
        set_state_if_changed(s_dev_page_switches[i], LV_STATE_DISABLED, switch_disabled);
        set_state_if_changed(s_dev_page_switches[i], LV_STATE_CHECKED, switch_checked);
    }
}

/* Shared device toggle logic */
static void toggle_device(uint8_t idx, bool is_retry)
{
    if (idx >= UI_DEVICE_COUNT) return;
    uint32_t now = lv_tick_get();

    /* Disconnected or already command pending -> refuse */
    if (!is_link_connected(now)) {
        ui_announce("Disconnected");
        return;
    }
    ui_device_cmd_state_t *cmd = &s_device_cmds[idx];
    if (cmd->phase == UI_CMD_PHASE_PENDING) {
        return;
    }

    const ui_hub_device_config_t *cfg = &s_dev_configs[idx];
    uint8_t reported_gpio = lora_node_status_get_relay_gpio(&lora_node_status, idx);
    bool reported_on = (reported_gpio ^ cfg->active_low) != 0;

    /* In Auto mode: Retry re-arms against fresh current report without blindly resending stale target */
    if (is_retry && cfg->mode == UI_MODE_AUTO) {
        ui_auto_rearm(&s_auto_states[idx], now);
        ui_auto_restart_hold(&s_auto_states[idx], now);
        cmd->phase = UI_CMD_PHASE_IDLE;
        s_desired_on[idx] = reported_on;
        lora_hub_cmd_set_relay_gpio(&lora_hub_cmd, idx, reported_gpio);
        ui_announce("Auto resumed");
        if (s_current_page == UI_PAGE_HOME) update_home_page_widgets();
        else if (s_current_page == UI_PAGE_DEVICES) update_devices_page_widgets();
        return;
    }

    /* Accepted user switch gesture in Auto atomically commits Manual mode */
    if (!is_retry && cfg->mode == UI_MODE_AUTO) {
        s_dev_configs[idx].mode = UI_MODE_MANUAL;
        ui_auto_rearm(&s_auto_states[idx], now);
        if (s_config_changed_cb != NULL) {
            s_config_changed_cb(idx, &s_dev_configs[idx]);
        }
    }

    if (!is_retry) {
        s_desired_on[idx] = !reported_on;
    }
    /* If retry in manual mode, keep current s_desired_on[idx] */

    uint8_t desired_gpio = (s_desired_on[idx] ^ cfg->active_low) ? 1u : 0u;
    lora_hub_cmd_set_relay_gpio(&lora_hub_cmd, idx, desired_gpio);
    lora_hub_cmd.bits.seq++;

    cmd->phase = UI_CMD_PHASE_PENDING;
    cmd->request_tick_ms = now;
    cmd->seq = lora_hub_cmd.bits.seq;

    ui_announce(is_retry ? "Retrying…" : "Sending…");

    /* Update in-place (no use-after-free, no page destruction) */
    if (s_current_page == UI_PAGE_HOME) update_home_page_widgets();
    else if (s_current_page == UI_PAGE_DEVICES) update_devices_page_widgets();
}

static void on_device_switch_click(lv_event_t *e)
{
    uint8_t idx = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
    lv_obj_t *sw = lv_event_get_target(e);
    toggle_device(idx, false);
    /* R10: If LVGL toggled switch before handler, immediately restore displayed readback */
    if (idx < UI_DEVICE_COUNT && sw != NULL) {
        uint8_t reported_gpio = lora_node_status_get_relay_gpio(&lora_node_status, idx);
        bool rep_on = (reported_gpio ^ s_dev_configs[idx].active_low) != 0;
        if (rep_on) {
            lv_obj_add_state(sw, LV_STATE_CHECKED);
        } else {
            lv_obj_clear_state(sw, LV_STATE_CHECKED);
        }
    }
}

static void on_device_retry_click(lv_event_t *e)
{
    uint8_t idx = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
    toggle_device(idx, true);
}

/* =========================================================================
 * FOCUSED SETTINGS EDITOR (shared draft, parent and Auto subviews)
 * ========================================================================= */
static void on_dialog_mode_btn_click(lv_event_t *e)
{
    uintptr_t mode = (uintptr_t)lv_event_get_user_data(e);
    if (mode == UI_MODE_AUTO && !ui_auto_is_preset_supported(s_dialog_edit_preset)) {
        return;
    }
    s_dialog_edit_mode = (ui_device_mode_t)mode;
    update_dialog_device_subview();
}


static void on_dialog_polarity_btn_click(lv_event_t *e)
{
    uintptr_t act_low = (uintptr_t)lv_event_get_user_data(e);
    s_dialog_edit_active_low = act_low ? 1u : 0u;
    if (s_dialog_active_low_sw != NULL) {
        if (s_dialog_edit_active_low) {
            lv_obj_add_state(s_dialog_active_low_sw, LV_STATE_CHECKED);
        } else {
            lv_obj_clear_state(s_dialog_active_low_sw, LV_STATE_CHECKED);
        }
    }
    update_dialog_device_subview();
}

static void on_dialog_open_limits_click(lv_event_t *e)
{
    (void)e;
    if (s_dialog_active_low_sw != NULL) {
        s_dialog_edit_active_low = lv_obj_has_state(s_dialog_active_low_sw, LV_STATE_CHECKED) ? 1u : 0u;
    }
    if (s_dialog_box != NULL) {
        build_dialog_auto_subview(s_dialog_box);
    }
}

static void on_dialog_limits_back_click(lv_event_t *e)
{
    (void)e;
    if (s_dialog_box != NULL) {
        build_dialog_device_subview(s_dialog_box);
    }
}

static void on_dialog_reset_limits_click(lv_event_t *e)
{
    (void)e;
    if (ui_auto_is_preset_supported(s_dialog_edit_preset)) {
        const ui_auto_rule_info_t *rule = ui_auto_get_rule_info(s_dialog_edit_preset);
        if (rule != NULL) {
            s_dialog_edit_on_thresh = rule->def_on;
            s_dialog_edit_off_thresh = rule->def_off;
            update_dialog_auto_subview();
        }
    }
}

static void step_threshold(bool is_on, int32_t dir)
{
    if (!ui_auto_is_preset_supported(s_dialog_edit_preset)) return;
    int32_t *target = is_on ? &s_dialog_edit_on_thresh : &s_dialog_edit_off_thresh;
    ui_auto_step_threshold(s_dialog_edit_preset, target, dir);
    update_dialog_auto_subview();
}

static void on_thresh_step_click(lv_event_t *e)
{
    uintptr_t data = (uintptr_t)lv_event_get_user_data(e);
    bool is_on = (data < 2u);
    int32_t dir = (data == 0u || data == 2u) ? 1 : -1;
    step_threshold(is_on, dir);
}

static void on_dialog_preset_change(lv_event_t *e)
{
    (void)e;
    if (s_dialog_preset_dd == NULL) return;
    uint8_t sel = (uint8_t)lv_dropdown_get_selected(s_dialog_preset_dd);
    s_dialog_edit_preset = sel;

    if (ui_auto_is_preset_supported(sel)) {
        const ui_auto_rule_info_t *rule = ui_auto_get_rule_info(sel);
        if (rule != NULL) {
            s_dialog_edit_on_thresh = rule->def_on;
            s_dialog_edit_off_thresh = rule->def_off;
        }
    } else {
        s_dialog_edit_mode = UI_MODE_MANUAL;
        s_dialog_edit_on_thresh = 0;
        s_dialog_edit_off_thresh = 0;
    }
    update_dialog_device_subview();
}

static void cleanup_dialog_dd_scrim(void)
{
    if (s_dialog_dd_scrim != NULL) {
        lv_obj_del(s_dialog_dd_scrim);
        s_dialog_dd_scrim = NULL;
    }
}

static void on_dropdown_scrim_click(lv_event_t *e)
{
    (void)e;
    if (s_dialog_btn_x != NULL) {
        lv_point_t p;
        lv_indev_get_point(lv_indev_get_act(), &p);
        lv_area_t x_coords;
        lv_obj_get_coords(s_dialog_btn_x, &x_coords);
        if (_lv_area_is_point_on(&x_coords, &p, 0)) {
            close_device_dialog();
            return;
        }
    }
    if (s_dialog_preset_dd != NULL && lv_dropdown_is_open(s_dialog_preset_dd)) {
        lv_dropdown_close(s_dialog_preset_dd);
    }
    cleanup_dialog_dd_scrim();
}

static void on_dialog_preset_dropdown_event(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_READY) {
        lv_obj_t *list = lv_dropdown_get_list(s_dialog_preset_dd);
        if (list != NULL && !ui_obj_has_style(list, &ui_style_dd_list, LV_PART_MAIN)) {
            lv_obj_add_style(list, &ui_style_dd_list, LV_PART_MAIN);
            lv_obj_add_style(list, &ui_style_dd_list_selected, LV_PART_SELECTED);
            lv_obj_add_style(list, &ui_style_dd_list_selected, LV_PART_SELECTED | LV_STATE_CHECKED);
            lv_obj_add_style(list, &ui_style_dd_list_selected, LV_PART_SELECTED | LV_STATE_PRESSED);
            lv_obj_add_style(list, &ui_style_dd_list_selected, LV_PART_SELECTED | LV_STATE_CHECKED | LV_STATE_PRESSED);
            lv_obj_t *lbl = lv_obj_get_child(list, 0);
            if (lbl != NULL && !ui_obj_has_style(lbl, &ui_style_dd_label, 0)) {
                lv_obj_add_style(lbl, &ui_style_dd_label, 0);
            }
        }
        if (s_dialog_dd_scrim == NULL && s_screen != NULL) {
            s_dialog_dd_scrim = lv_obj_create(s_screen);
            lv_obj_remove_style_all(s_dialog_dd_scrim);
            lv_obj_set_size(s_dialog_dd_scrim, 800, 480);
            lv_obj_add_flag(s_dialog_dd_scrim, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_event_cb(s_dialog_dd_scrim, on_dropdown_scrim_click, LV_EVENT_CLICKED, NULL);
            if (list != NULL) {
                lv_obj_move_foreground(list);
            }
            if (s_dialog_btn_x != NULL) {
                lv_obj_move_foreground(s_dialog_btn_x);
            }
        }
    } else if (code == LV_EVENT_CANCEL || code == LV_EVENT_VALUE_CHANGED) {
        cleanup_dialog_dd_scrim();
    }
}

static void update_dialog_device_subview(void)
{
    if (s_dialog_subview_device == NULL) return;

    if (s_dialog_device_title_lbl != NULL) {
        static char s_dialog_title_buf[64];
        const char *disp_name = (s_dialog_edit_preset == UI_PRESET_FAN) ? "Ventilation" : s_presets[s_dialog_edit_preset].name;
        snprintf(s_dialog_title_buf, sizeof(s_dialog_title_buf), "%s · Settings", disp_name);
        lv_label_set_text_static(s_dialog_device_title_lbl, s_dialog_title_buf);
    }
    if (s_dialog_device_icon != NULL) {
        lv_img_set_src(s_dialog_device_icon, get_preset_icon_24(s_dialog_edit_preset));
    }

    bool auto_supported = ui_auto_is_preset_supported(s_dialog_edit_preset);
    if (!auto_supported) {
        s_dialog_edit_mode = UI_MODE_MANUAL;
        if (s_dialog_mode_btns[1] != NULL) {
            lv_obj_add_state(s_dialog_mode_btns[1], LV_STATE_DISABLED);
        }
    } else {
        if (s_dialog_mode_btns[1] != NULL) {
            lv_obj_clear_state(s_dialog_mode_btns[1], LV_STATE_DISABLED);
        }
    }

    /* Mode capsule buttons */
    for (int i = 0; i < 2; i++) {
        if (s_dialog_mode_btns[i] == NULL) continue;
        bool is_sel = (s_dialog_btn_modes[i] == s_dialog_edit_mode);
        if (i == 0) {
            set_bg_color_if_changed(s_dialog_mode_btns[i], is_sel ? UI_COLOR_FOREST : UI_COLOR_CANVAS);
            set_bg_opa_if_changed(s_dialog_mode_btns[i], is_sel ? LV_OPA_COVER : LV_OPA_TRANSP);
            set_text_color_if_changed(s_dialog_mode_labels[i], is_sel ? lv_color_white() : UI_COLOR_INK);
        } else {
            if (is_sel) {
                set_bg_color_if_changed(s_dialog_mode_btns[i], UI_COLOR_FOREST);
                set_bg_opa_if_changed(s_dialog_mode_btns[i], LV_OPA_COVER);
                set_text_color_if_changed(s_dialog_mode_labels[i], lv_color_white());
            } else {
                set_bg_color_if_changed(s_dialog_mode_btns[i], UI_COLOR_CANVAS);
                set_bg_opa_if_changed(s_dialog_mode_btns[i], LV_OPA_TRANSP);
                set_text_color_if_changed(s_dialog_mode_labels[i], auto_supported ? UI_COLOR_INK : UI_COLOR_MUTED);
            }
        }
    }

    /* Row 2: Auto thresholds */
    if (auto_supported) {
        const ui_auto_rule_info_t *rule = ui_auto_get_rule_info(s_dialog_edit_preset);
        bool limits_valid = ui_auto_validate_thresholds(s_dialog_edit_preset,
                            s_dialog_edit_on_thresh, s_dialog_edit_off_thresh, NULL, 0);
        const char *unit = (s_dialog_edit_preset == UI_PRESET_HUMIDIFIER) ? "%" : rule->unit_name;
        if (limits_valid) {
            snprintf(s_dialog_limits_summary, sizeof(s_dialog_limits_summary),
                     "%d / %d %s", (int)s_dialog_edit_on_thresh,
                     (int)s_dialog_edit_off_thresh, unit);
            if (s_dialog_limits_summary_lbl != NULL) {
                set_text_color_if_changed(s_dialog_limits_summary_lbl, UI_COLOR_INK);
            }
        } else {
            snprintf(s_dialog_limits_summary, sizeof(s_dialog_limits_summary), "Fix invalid limits");
            if (s_dialog_limits_summary_lbl != NULL) {
                set_text_color_if_changed(s_dialog_limits_summary_lbl, UI_COLOR_RED);
            }
        }
        if (s_dialog_limits_summary_lbl != NULL) {
            lv_label_set_text_static(s_dialog_limits_summary_lbl, s_dialog_limits_summary);
        }
        if (s_dialog_limits_chevron != NULL) lv_obj_clear_flag(s_dialog_limits_chevron, LV_OBJ_FLAG_HIDDEN);
        if (s_dialog_btn_limits != NULL) {
            lv_obj_add_flag(s_dialog_btn_limits, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_clear_state(s_dialog_btn_limits, LV_STATE_DISABLED);
        }
    } else {
        snprintf(s_dialog_limits_summary, sizeof(s_dialog_limits_summary), "Manual only");
        if (s_dialog_limits_summary_lbl != NULL) {
            lv_label_set_text_static(s_dialog_limits_summary_lbl, s_dialog_limits_summary);
            set_text_color_if_changed(s_dialog_limits_summary_lbl, UI_COLOR_MUTED);
        }
        if (s_dialog_limits_chevron != NULL) lv_obj_add_flag(s_dialog_limits_chevron, LV_OBJ_FLAG_HIDDEN);
        if (s_dialog_btn_limits != NULL) {
            lv_obj_clear_flag(s_dialog_btn_limits, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_state(s_dialog_btn_limits, LV_STATE_DISABLED);
        }
    }

    /* Row 3: Output polarity segmented buttons */
    if (s_dialog_active_low_btns[0] != NULL && s_dialog_active_low_btns[1] != NULL) {
        bool act_low = (s_dialog_active_low_sw != NULL) ?
                       lv_obj_has_state(s_dialog_active_low_sw, LV_STATE_CHECKED) : (s_dialog_edit_active_low != 0);
        s_dialog_edit_active_low = act_low ? 1u : 0u;

        /* Active high: button 0 */
        set_bg_color_if_changed(s_dialog_active_low_btns[0], act_low ? UI_COLOR_CANVAS : UI_COLOR_CELADON);
        set_bg_opa_if_changed(s_dialog_active_low_btns[0], act_low ? LV_OPA_TRANSP : LV_OPA_COVER);
        set_text_color_if_changed(s_dialog_active_low_labels[0], UI_COLOR_INK);

        /* Active low: button 1 */
        set_bg_color_if_changed(s_dialog_active_low_btns[1], act_low ? UI_COLOR_CELADON : UI_COLOR_CANVAS);
        set_bg_opa_if_changed(s_dialog_active_low_btns[1], act_low ? LV_OPA_COVER : LV_OPA_TRANSP);
        set_text_color_if_changed(s_dialog_active_low_labels[1], UI_COLOR_INK);
    }

    /* Save button validation */
    if (s_dialog_btn_save_dev != NULL && s_dialog_device_idx >= 0 && (uint8_t)s_dialog_device_idx < UI_DEVICE_COUNT) {
        ui_hub_device_config_t cand = s_dev_configs[s_dialog_device_idx];
        cand.preset = s_dialog_edit_preset;
        cand.mode = s_dialog_edit_mode;
        cand.active_low = s_dialog_edit_active_low;
        cand.auto_on_thresh = s_dialog_edit_on_thresh;
        cand.auto_off_thresh = s_dialog_edit_off_thresh;
        cand.schema_version = UI_CONFIG_SCHEMA_VERSION;
        if (cand.preset != s_dev_configs[s_dialog_device_idx].preset) {
            snprintf(cand.name, sizeof(cand.name), "%s", s_presets[cand.preset].name);
        }

        char err_buf[64];
        bool valid = ui_auto_validate_config(&cand, err_buf, sizeof(err_buf));
        set_text_color_if_changed(lv_obj_get_child(s_dialog_btn_save_dev, 0),
                                  valid ? lv_color_white() : UI_COLOR_MUTED);
        if (valid) {
            lv_obj_clear_state(s_dialog_btn_save_dev, LV_STATE_DISABLED);
        } else {
            lv_obj_add_state(s_dialog_btn_save_dev, LV_STATE_DISABLED);
        }
    }
}

static void update_dialog_live_reading(void)
{
    if (s_dialog_live_lbl == NULL) return;
    const ui_auto_rule_info_t *rule = ui_auto_get_rule_info(s_dialog_edit_preset);
    if (!rule) return;
    uint32_t now = lv_tick_get();
    bool valid = false;
    int value = 0;
    switch (s_dialog_edit_preset) {
        case UI_PRESET_FAN:
            valid = ui_is_sensor_available(UI_METRIC_CO2, now);
            value = lora_node_status.bits.co2_ppm; break;
        case UI_PRESET_PURIFIER:
            valid = ui_is_sensor_available(UI_METRIC_VOC, now);
            value = lora_node_status.bits.tvoc_ppb; break;
        case UI_PRESET_HUMIDIFIER:
            valid = ui_is_sensor_available(UI_METRIC_HUMIDITY, now);
            value = lora_node_status.bits.humidity_pct; break;
        default: valid = false; break;
    }
    const char *display_unit = (s_dialog_edit_preset == UI_PRESET_HUMIDIFIER) ? "%" : rule->unit_name;
    char text[32];
    if (valid) {
        snprintf(text, sizeof(text), "%d", value);
        set_text_color_if_changed(s_dialog_live_lbl, UI_COLOR_INK);
        set_buffer_and_label_if_changed(s_dialog_live_lbl, s_dialog_live_buf, sizeof(s_dialog_live_buf), text);
        if (s_dialog_live_unit_lbl != NULL) {
            set_buffer_and_label_if_changed(s_dialog_live_unit_lbl, s_dialog_live_unit_buf, sizeof(s_dialog_live_unit_buf), display_unit);
            lv_point_t sz;
            lv_txt_get_size(&sz, s_dialog_live_buf, &ui_font_34, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
            set_pos_if_changed(s_dialog_live_unit_lbl, 490 + sz.x + 6, 45);
            set_flag_if_changed(s_dialog_live_unit_lbl, LV_OBJ_FLAG_HIDDEN, display_unit[0] == '\0');
        }
    } else {
        snprintf(text, sizeof(text), "No live data");
        set_text_color_if_changed(s_dialog_live_lbl, UI_COLOR_MUTED);
        set_buffer_and_label_if_changed(s_dialog_live_lbl, s_dialog_live_buf, sizeof(s_dialog_live_buf), text);
        if (s_dialog_live_unit_lbl != NULL) {
            set_buffer_and_label_if_changed(s_dialog_live_unit_lbl, s_dialog_live_unit_buf, sizeof(s_dialog_live_unit_buf), "");
            set_flag_if_changed(s_dialog_live_unit_lbl, LV_OBJ_FLAG_HIDDEN, true);
        }
    }
}

static void update_dialog_auto_subview(void)
{
    if (s_dialog_subview_auto == NULL) return;
    const ui_auto_rule_info_t *rule = ui_auto_get_rule_info(s_dialog_edit_preset);
    if (rule == NULL) return;

    /* Celadon strip title */
    if (s_dialog_edit_preset == UI_PRESET_FAN) {
        snprintf(s_dialog_auto_rule_buf, sizeof(s_dialog_auto_rule_buf), "eCO₂");
    } else if (s_dialog_edit_preset == UI_PRESET_PURIFIER) {
        snprintf(s_dialog_auto_rule_buf, sizeof(s_dialog_auto_rule_buf), "TVOC");
    } else if (s_dialog_edit_preset == UI_PRESET_HUMIDIFIER) {
        snprintf(s_dialog_auto_rule_buf, sizeof(s_dialog_auto_rule_buf), "Humidity");
    } else {
        snprintf(s_dialog_auto_rule_buf, sizeof(s_dialog_auto_rule_buf), "%s", rule->sensor_name);
    }
    lv_label_set_text_static(s_dialog_auto_rule_lbl, s_dialog_auto_rule_buf);

    bool inverse = (s_dialog_edit_preset == UI_PRESET_HUMIDIFIER);
    snprintf(s_dialog_on_op_buf, sizeof(s_dialog_on_op_buf), "%s at or %s",
             s_dialog_auto_rule_buf, inverse ? "below" : "above");
    snprintf(s_dialog_off_op_buf, sizeof(s_dialog_off_op_buf), "%s at or %s",
             s_dialog_auto_rule_buf, inverse ? "above" : "below");
    lv_label_set_text_static(s_dialog_on_op_lbl, s_dialog_on_op_buf);
    lv_label_set_text_static(s_dialog_off_op_lbl, s_dialog_off_op_buf);

    update_dialog_live_reading();

    const char *display_unit = inverse ? "%" : rule->unit_name;
    snprintf(s_dialog_on_val_buf, sizeof(s_dialog_on_val_buf), "%d",
             (int)s_dialog_edit_on_thresh);
    snprintf(s_dialog_off_val_buf, sizeof(s_dialog_off_val_buf), "%d",
             (int)s_dialog_edit_off_thresh);
    snprintf(s_dialog_on_unit_buf, sizeof(s_dialog_on_unit_buf), "%s", display_unit);
    snprintf(s_dialog_off_unit_buf, sizeof(s_dialog_off_unit_buf), "%s", display_unit);
    lv_label_set_text_static(s_dialog_on_val_lbl, s_dialog_on_val_buf);
    lv_label_set_text_static(s_dialog_off_val_lbl, s_dialog_off_val_buf);
    if (s_dialog_on_unit_lbl != NULL) lv_label_set_text_static(s_dialog_on_unit_lbl, s_dialog_on_unit_buf);
    if (s_dialog_off_unit_lbl != NULL) lv_label_set_text_static(s_dialog_off_unit_lbl, s_dialog_off_unit_buf);

    /* Baseline and horizontal centering within val_box (width 212, height 48) */
    bool has_unit = (display_unit[0] != '\0');
    lv_point_t unit_sz = {0, 0};
    if (has_unit) {
        lv_txt_get_size(&unit_sz, display_unit, &ui_font_18, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    }
    if (s_dialog_on_val_lbl != NULL) {
        lv_point_t val_sz;
        lv_txt_get_size(&val_sz, s_dialog_on_val_buf, &ui_font_34, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
        lv_coord_t total_w = val_sz.x + (has_unit ? (unit_sz.x + 8) : 0);
        lv_coord_t start_x = (212 - total_w) / 2;
        if (start_x < 4) start_x = 4;
        set_pos_if_changed(s_dialog_on_val_lbl, start_x, 0);
        if (s_dialog_on_unit_lbl != NULL) {
            set_pos_if_changed(s_dialog_on_unit_lbl, start_x + val_sz.x + 8, 14);
            set_flag_if_changed(s_dialog_on_unit_lbl, LV_OBJ_FLAG_HIDDEN, !has_unit);
        }
    }
    if (s_dialog_off_val_lbl != NULL) {
        lv_point_t val_sz;
        lv_txt_get_size(&val_sz, s_dialog_off_val_buf, &ui_font_34, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
        lv_coord_t total_w = val_sz.x + (has_unit ? (unit_sz.x + 8) : 0);
        lv_coord_t start_x = (212 - total_w) / 2;
        if (start_x < 4) start_x = 4;
        set_pos_if_changed(s_dialog_off_val_lbl, start_x, 0);
        if (s_dialog_off_unit_lbl != NULL) {
            set_pos_if_changed(s_dialog_off_unit_lbl, start_x + val_sz.x + 8, 14);
            set_flag_if_changed(s_dialog_off_unit_lbl, LV_OBJ_FLAG_HIDDEN, !has_unit);
        }
    }

    char err_msg[64] = "";
    bool valid = ui_auto_validate_thresholds(s_dialog_edit_preset, s_dialog_edit_on_thresh, s_dialog_edit_off_thresh, err_msg, sizeof(err_msg));

    /* Update validation slot without shifting content (Fix F5 P2) */
    if (valid) {
        const char *hint = inverse ?
            "OFF must be at least 5 % above ON.\nBetween limits, the device keeps its state." :
            "ON must be at least 50 ppm above OFF.\nBetween limits, the device keeps its state.";
        if (s_dialog_edit_preset == UI_PRESET_PURIFIER) {
            hint = "ON must be at least 5 above OFF.\nBetween limits, the device keeps its state.";
        }
        snprintf(s_dialog_validation_buf, sizeof(s_dialog_validation_buf), "%s", hint);
        if (s_dialog_warn_ico != NULL) lv_obj_add_flag(s_dialog_warn_ico, LV_OBJ_FLAG_HIDDEN);
        if (s_dialog_validation_lbl != NULL) {
            lv_obj_set_pos(s_dialog_validation_lbl, 8, 4);
            lv_obj_set_width(s_dialog_validation_lbl, 736);
            set_text_color_if_changed(s_dialog_validation_lbl, UI_COLOR_MUTED);
            lv_label_set_text_static(s_dialog_validation_lbl, s_dialog_validation_buf);
        }
        if (s_last_dialog_valid_state != 1) {
            s_last_dialog_valid_state = 1;
            if (s_dialog_on_val_box != NULL) {
                lv_obj_remove_style(s_dialog_on_val_box, &ui_style_field_error, 0);
                lv_obj_add_style(s_dialog_on_val_box, &ui_style_field_outlined, 0);
            }
            if (s_dialog_off_val_box != NULL) {
                lv_obj_remove_style(s_dialog_off_val_box, &ui_style_field_error, 0);
                lv_obj_add_style(s_dialog_off_val_box, &ui_style_field_outlined, 0);
            }
        }
    } else {
        const char *repair = inverse ?
            "OFF must be at least 5 %RH above ON.\nRaise OFF or lower ON to continue." :
            (s_dialog_edit_preset == UI_PRESET_FAN ?
             "ON must be at least 50 ppm above OFF.\nRaise ON or lower OFF to continue." :
             "ON must be at least 5 above OFF.\nRaise ON or lower OFF to continue.");
        snprintf(s_dialog_validation_buf, sizeof(s_dialog_validation_buf), "%s", repair);
        if (s_dialog_warn_ico != NULL) lv_obj_clear_flag(s_dialog_warn_ico, LV_OBJ_FLAG_HIDDEN);
        if (s_dialog_validation_lbl != NULL) {
            lv_obj_set_pos(s_dialog_validation_lbl, 40, 0);
            lv_obj_set_width(s_dialog_validation_lbl, 704);
            set_text_color_if_changed(s_dialog_validation_lbl, UI_COLOR_ERROR);
            lv_label_set_text_static(s_dialog_validation_lbl, s_dialog_validation_buf);
        }
        if (s_last_dialog_valid_state != 0) {
            s_last_dialog_valid_state = 0;
            if (s_dialog_on_val_box != NULL) {
                lv_obj_remove_style(s_dialog_on_val_box, &ui_style_field_outlined, 0);
                lv_obj_add_style(s_dialog_on_val_box, &ui_style_field_error, 0);
            }
            if (s_dialog_off_val_box != NULL) {
                lv_obj_remove_style(s_dialog_off_val_box, &ui_style_field_outlined, 0);
                lv_obj_add_style(s_dialog_off_val_box, &ui_style_field_error, 0);
            }
        }
    }

    set_state_if_changed(s_dialog_btn_on_minus, LV_STATE_DISABLED, s_dialog_edit_on_thresh <= rule->min_val);
    set_state_if_changed(s_dialog_btn_on_plus, LV_STATE_DISABLED, s_dialog_edit_on_thresh >= rule->max_val);
    set_state_if_changed(s_dialog_btn_off_minus, LV_STATE_DISABLED, s_dialog_edit_off_thresh <= rule->min_val);
    set_state_if_changed(s_dialog_btn_off_plus, LV_STATE_DISABLED, s_dialog_edit_off_thresh >= rule->max_val);

    if (s_dialog_btn_save_auto != NULL) {
        set_text_color_if_changed(lv_obj_get_child(s_dialog_btn_save_auto, 0),
                                  valid ? lv_color_white() : UI_COLOR_MUTED);
        if (valid) {
            lv_obj_clear_state(s_dialog_btn_save_auto, LV_STATE_DISABLED);
        } else {
            lv_obj_add_state(s_dialog_btn_save_auto, LV_STATE_DISABLED);
        }
    }
}

static bool commit_device_config_internal(uint8_t idx, const ui_hub_device_config_t *cand, bool is_boot_staging, char *err_msg, size_t err_sz)
{
    if (idx >= UI_DEVICE_COUNT || cand == NULL) {
        if (err_msg && err_sz > 0) snprintf(err_msg, err_sz, "Invalid device or config");
        return false;
    }

    if (!ui_auto_validate_config(cand, err_msg, err_sz)) {
        if (is_boot_staging) {
            /* Fall back to safe defaults on invalid storage at boot */
            ui_auto_get_defaults(idx, (cand->preset < UI_PRESET_COUNT) ? cand->preset : UI_PRESET_GENERIC, &s_dev_configs[idx]);
            s_dev_configured[idx] = true;
        }
        return false;
    }

    if (is_boot_staging) {
        s_dev_configs[idx] = *cand;
        s_dev_configured[idx] = true;
        return true;
    }

    /* Runtime commit: refuse live polarity or preset changes while command is in flight (R3) */
    if (s_device_cmds[idx].phase == UI_CMD_PHASE_PENDING) {
        if (cand->active_low != s_dev_configs[idx].active_low) {
            if (err_msg && err_sz > 0) snprintf(err_msg, err_sz, "Cannot change polarity while command is pending.");
            return false;
        }
        if (cand->preset != s_dev_configs[idx].preset) {
            if (err_msg && err_sz > 0) snprintf(err_msg, err_sz, "Cannot change device type while command is pending.");
            return false;
        }
    }

    const ui_hub_device_config_t *cur = &s_dev_configs[idx];
    bool limits_changed = (cand->auto_on_thresh != cur->auto_on_thresh ||
                           cand->auto_off_thresh != cur->auto_off_thresh);
    bool mode_changed = (cand->mode != cur->mode);
    bool polarity_changed = (cand->active_low != cur->active_low);
    bool preset_changed = (cand->preset != cur->preset);
    bool name_changed = (strncmp(cand->name, cur->name, sizeof(cand->name)) != 0);

    if (!limits_changed && !mode_changed && !polarity_changed && !preset_changed && !name_changed) {
        /* No-op Save: does not reset state or invoke persistence again */
        return true;
    }

    uint32_t now = lv_tick_get();

    /* Commit configuration locally */
    s_dev_configs[idx] = *cand;
    s_dev_configured[idx] = true;

    if (mode_changed && cand->mode == UI_MODE_AUTO) {
        ui_auto_rearm(&s_auto_states[idx], now);
    } else if (mode_changed && cand->mode == UI_MODE_MANUAL) {
        ui_auto_rearm(&s_auto_states[idx], now);
    } else if (limits_changed && cand->mode == UI_MODE_AUTO) {
        ui_auto_reset_qualification(&s_auto_states[idx]);
    }

    /* If polarity changed live and link online, recompute and send wire GPIO */
    if (polarity_changed && is_link_connected(now)) {
        uint8_t new_gpio = (s_desired_on[idx] ^ cand->active_low) ? 1u : 0u;
        uint8_t cur_gpio = lora_hub_cmd_get_relay_gpio(&lora_hub_cmd, idx);
        if (new_gpio != cur_gpio) {
            lora_hub_cmd_set_relay_gpio(&lora_hub_cmd, idx, new_gpio);
            lora_hub_cmd.bits.seq++;
            s_device_cmds[idx].phase = UI_CMD_PHASE_PENDING;
            s_device_cmds[idx].request_tick_ms = now;
            s_device_cmds[idx].seq = lora_hub_cmd.bits.seq;
        }
    }

    if (s_screen != NULL) {
        if (s_current_page == UI_PAGE_HOME) update_home_page_widgets();
        else if (s_current_page == UI_PAGE_DEVICES) update_devices_page_widgets();
    }

    if (s_config_changed_cb != NULL) {
        s_config_changed_cb(idx, &s_dev_configs[idx]);
    }

    return true;
}

static void on_dialog_save_click(lv_event_t *e)
{
    (void)e;
    if (s_dialog_device_idx < 0 || (uint8_t)s_dialog_device_idx >= UI_DEVICE_COUNT) {
        close_device_dialog();
        return;
    }

    uint8_t idx = (uint8_t)s_dialog_device_idx;

    if (s_dialog_active_low_sw != NULL) {
        s_dialog_edit_active_low = lv_obj_has_state(s_dialog_active_low_sw, LV_STATE_CHECKED) ? 1u : 0u;
    }

    ui_hub_device_config_t cand = s_dev_configs[idx];
    cand.preset = s_dialog_edit_preset;
    cand.mode = s_dialog_edit_mode;
    cand.active_low = s_dialog_edit_active_low;
    cand.auto_on_thresh = s_dialog_edit_on_thresh;
    cand.auto_off_thresh = s_dialog_edit_off_thresh;
    cand.schema_version = UI_CONFIG_SCHEMA_VERSION;
    if (cand.preset != s_dev_configs[idx].preset) {
        snprintf(cand.name, sizeof(cand.name), "%s", s_presets[cand.preset].name);
    }

    char err_msg[64] = {0};
    if (!commit_device_config_internal(idx, &cand, false, err_msg, sizeof(err_msg))) {
        snprintf(s_dialog_validation_buf, sizeof(s_dialog_validation_buf), "%s",
                 err_msg[0] ? err_msg : "Invalid settings draft.");
        if (!s_dialog_validation_lbl && s_dialog_subview_device) {
            s_dialog_validation_lbl = add_static_label(s_dialog_subview_device,
                                         s_dialog_validation_buf, &ui_font_18, UI_COLOR_RED);
            lv_obj_set_pos(s_dialog_validation_lbl, 24, 332);
            lv_obj_set_size(s_dialog_validation_lbl, 704, 18);
        }
        if (s_dialog_validation_lbl) {
            lv_label_set_text_static(s_dialog_validation_lbl, s_dialog_validation_buf);
            set_text_color_if_changed(s_dialog_validation_lbl, UI_COLOR_RED);
            lv_obj_clear_flag(s_dialog_validation_lbl, LV_OBJ_FLAG_HIDDEN);
        }
        return;
    }

    close_device_dialog();
    ui_announce("Settings saved");
}

static void on_dialog_close_click(lv_event_t *e)
{
    (void)e;
    close_device_dialog();
}

static void build_dialog_device_subview(lv_obj_t *box)
{
    if (box == NULL) return;

    if (s_dialog_subview_auto != NULL) {
        lv_obj_del(s_dialog_subview_auto);
        s_dialog_subview_auto = NULL;
        s_dialog_auto_rule_lbl = NULL;
        s_dialog_on_op_lbl = NULL;
        s_dialog_off_op_lbl = NULL;
        s_dialog_on_val_lbl = NULL;
        s_dialog_off_val_lbl = NULL;
        s_dialog_on_unit_lbl = NULL;
        s_dialog_off_unit_lbl = NULL;
        s_dialog_live_unit_lbl = NULL;
        s_dialog_validation_lbl = NULL;
        s_dialog_live_lbl = NULL;
        s_dialog_limit_cards[0] = s_dialog_limit_cards[1] = NULL;
        s_dialog_btn_on_minus = s_dialog_btn_on_plus = NULL;
        s_dialog_btn_off_minus = s_dialog_btn_off_plus = NULL;
        s_dialog_on_val_box = s_dialog_off_val_box = NULL;
        s_dialog_warn_ico = NULL;
    }
    if (s_dialog_btn_reset != NULL) {
        lv_obj_del(s_dialog_btn_reset);
        s_dialog_btn_reset = NULL;
    }
    if (s_dialog_btn_x != NULL) {
        lv_obj_set_y(s_dialog_btn_x, 352);
    }
    if (s_dialog_btn_save_dev != NULL) {
        lv_obj_set_y(s_dialog_btn_save_dev, 352);
    }

    s_dialog_subview_device = lv_obj_create(box);
    lv_obj_remove_style_all(s_dialog_subview_device);
    lv_obj_set_pos(s_dialog_subview_device, 0, 0);
    lv_obj_set_size(s_dialog_subview_device, 752, 350);
    lv_obj_clear_flag(s_dialog_subview_device, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    /* Back button at (0, 10, 44, 44) - unboxed chevron matching cap height */
    lv_obj_t *back = lv_btn_create(s_dialog_subview_device);
    lv_obj_remove_style_all(back);
    lv_obj_set_pos(back, 0, 10);
    lv_obj_set_size(back, 44, 44);
    lv_obj_add_style(back, &ui_style_pressed, LV_STATE_PRESSED);
    lv_obj_add_event_cb(back, on_dialog_close_click, LV_EVENT_CLICKED, NULL);
    lv_obj_t *back_ico = add_icon(back, &ui_icon_chevron_left_20, UI_COLOR_INK);
    lv_obj_center(back_ico);

    /* Contextual Title at (44, 16) with ui_font_24 */
    static char s_dev_title_buf[64];
    const char *disp_name = (s_dialog_edit_preset == UI_PRESET_FAN) ? "Ventilation" : s_presets[s_dialog_edit_preset].name;
    snprintf(s_dev_title_buf, sizeof(s_dev_title_buf), "%s · Settings", disp_name);
    s_dialog_device_title_lbl = add_static_label(s_dialog_subview_device, s_dev_title_buf, &ui_font_24, UI_COLOR_INK);
    lv_obj_set_pos(s_dialog_device_title_lbl, 44, 16);

    /* Porcelain Form Card (pos: 0, 68, size: 752, 264) - 4 balanced 66px rows */
    lv_obj_t *form = lv_obj_create(s_dialog_subview_device);
    lv_obj_remove_style_all(form);
    lv_obj_set_pos(form, 0, 68);
    lv_obj_set_size(form, 752, 264);
    lv_obj_add_style(form, &ui_style_card_porcelain, 0);
    lv_obj_clear_flag(form, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

    /* Row 0 (y=0..66): Device type (24px icon centered in 66px row) */
    s_dialog_device_icon = add_icon(form, get_preset_icon_24(s_dialog_edit_preset), UI_COLOR_INK);
    lv_obj_set_pos(s_dialog_device_icon, 20, 21);
    lv_obj_t *lbl_t = add_static_label(form, "Device type", &ui_font_18, UI_COLOR_INK);
    lv_obj_set_pos(lbl_t, 60, 22);

    s_dialog_preset_dd = lv_dropdown_create(form);
    lv_obj_set_pos(s_dialog_preset_dd, 440, 10);
    lv_obj_set_size(s_dialog_preset_dd, 292, 46);
    lv_obj_add_style(s_dialog_preset_dd, &ui_style_dialog_dropdown, 0);
    lv_dropdown_set_symbol(s_dialog_preset_dd, &ui_icon_chevron_down_18);
    lv_dropdown_set_options_static(s_dialog_preset_dd,
        "Air Purifier\n"
        "Ventilation Fan\n"
        "Humidifier\n"
        "Dehumidifier\n"
        "Heater\n"
        "Desk Light\n"
        "Smart Socket\n"
        "Generic Device");
    lv_dropdown_set_selected(s_dialog_preset_dd, s_dialog_edit_preset);
    lv_obj_add_event_cb(s_dialog_preset_dd, on_dialog_preset_change, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(s_dialog_preset_dd, on_dialog_preset_dropdown_event, LV_EVENT_ALL, NULL);

    /* Divider 0 */
    lv_obj_t *div0 = lv_obj_create(form);
    lv_obj_remove_style_all(div0);
    lv_obj_set_pos(div0, 20, 66);
    lv_obj_set_size(div0, 712, 1);
    lv_obj_set_style_bg_color(div0, UI_COLOR_DIVIDER, 0);
    lv_obj_set_style_bg_opa(div0, LV_OPA_COVER, 0);

    /* Row 1 (y=66..132): Control mode (24px icon centered in 66px row) */
    lv_obj_t *ico_m = add_icon(form, &ui_icon_sliders_24, UI_COLOR_INK);
    lv_obj_set_pos(ico_m, 20, 87);
    lv_obj_t *lbl_m = add_static_label(form, "Control mode", &ui_font_18, UI_COLOR_INK);
    lv_obj_set_pos(lbl_m, 60, 88);

    lv_obj_t *mode_pill = lv_obj_create(form);
    lv_obj_remove_style_all(mode_pill);
    lv_obj_set_pos(mode_pill, 440, 76);
    lv_obj_set_size(mode_pill, 292, 46);
    lv_obj_set_style_bg_color(mode_pill, UI_COLOR_CANVAS, 0);
    lv_obj_set_style_bg_opa(mode_pill, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(mode_pill, 10, 0);
    lv_obj_set_style_border_width(mode_pill, 1, 0);
    lv_obj_set_style_border_color(mode_pill, UI_COLOR_OUTLINE, 0);
    lv_obj_clear_flag(mode_pill, LV_OBJ_FLAG_SCROLLABLE);

    const char *modes[2] = { "Manual", "Auto" };
    for (uintptr_t i = 0; i < 2; i++) {
        lv_obj_t *b = lv_btn_create(mode_pill);
        lv_obj_remove_style_all(b);
        lv_obj_set_pos(b, (lv_coord_t)(i * 146), 0);
        lv_obj_set_size(b, 146, 46);
        lv_obj_set_style_radius(b, 9, 0);
        lv_obj_add_style(b, &ui_style_pressed, LV_STATE_PRESSED);
        lv_obj_add_style(b, &ui_style_action_disabled, LV_STATE_DISABLED);
        lv_obj_add_event_cb(b, on_dialog_mode_btn_click, LV_EVENT_CLICKED, (void *)(uintptr_t)s_dialog_btn_modes[i]);

        s_dialog_mode_labels[i] = add_static_label(b, modes[i], &ui_font_18, UI_COLOR_INK);
        lv_obj_center(s_dialog_mode_labels[i]);
        s_dialog_mode_btns[i] = b;
    }

    /* Divider 1 */
    lv_obj_t *div1 = lv_obj_create(form);
    lv_obj_remove_style_all(div1);
    lv_obj_set_pos(div1, 20, 132);
    lv_obj_set_size(div1, 712, 1);
    lv_obj_set_style_bg_color(div1, UI_COLOR_DIVIDER, 0);
    lv_obj_set_style_bg_opa(div1, LV_OPA_COVER, 0);

    /* Row 2 (y=132..198): Auto thresholds */
    s_dialog_btn_limits = lv_btn_create(form);
    lv_obj_remove_style_all(s_dialog_btn_limits);
    lv_obj_set_pos(s_dialog_btn_limits, 0, 132);
    lv_obj_set_size(s_dialog_btn_limits, 752, 66);
    lv_obj_add_style(s_dialog_btn_limits, &ui_style_pressed, LV_STATE_PRESSED);
    lv_obj_add_flag(s_dialog_btn_limits, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_dialog_btn_limits, on_dialog_open_limits_click, LV_EVENT_CLICKED, NULL);

    /* Child 0 MUST be the summary label for test hook compatibility (CHECK 32) */
    s_dialog_limits_summary_lbl = add_static_label(s_dialog_btn_limits, s_dialog_limits_summary, &ui_font_18, UI_COLOR_INK);
    lv_obj_set_pos(s_dialog_limits_summary_lbl, 440, 21);
    lv_obj_set_size(s_dialog_limits_summary_lbl, 256, 24);
    lv_obj_set_style_text_align(s_dialog_limits_summary_lbl, LV_TEXT_ALIGN_RIGHT, 0);

    lv_obj_t *ico_leaf = add_icon(s_dialog_btn_limits, &ui_icon_leaf_24, UI_COLOR_INK);
    lv_obj_set_pos(ico_leaf, 20, 21);
    lv_obj_clear_flag(ico_leaf, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *lbl_lim_title = add_static_label(s_dialog_btn_limits, "Auto thresholds", &ui_font_18, UI_COLOR_INK);
    lv_obj_set_pos(lbl_lim_title, 60, 22);
    lv_obj_clear_flag(lbl_lim_title, LV_OBJ_FLAG_CLICKABLE);

    s_dialog_limits_chevron = add_icon(s_dialog_btn_limits, &ui_icon_chevron_right_20, UI_COLOR_MUTED);
    lv_obj_set_pos(s_dialog_limits_chevron, 712, 23);
    lv_obj_clear_flag(s_dialog_limits_chevron, LV_OBJ_FLAG_CLICKABLE);

    s_dialog_mode_helper_lbl = NULL;

    /* Divider 2 */
    lv_obj_t *div2 = lv_obj_create(form);
    lv_obj_remove_style_all(div2);
    lv_obj_set_pos(div2, 20, 198);
    lv_obj_set_size(div2, 712, 1);
    lv_obj_set_style_bg_color(div2, UI_COLOR_DIVIDER, 0);
    lv_obj_set_style_bg_opa(div2, LV_OPA_COVER, 0);

    /* Row 3 (y=198..264): Output polarity (24px icon centered in 66px row) */
    lv_obj_t *ico_pol = add_icon(form, &ui_icon_gear_24, UI_COLOR_INK);
    lv_obj_set_pos(ico_pol, 20, 219);
    lv_obj_t *lbl_pol = add_static_label(form, "Output polarity", &ui_font_18, UI_COLOR_INK);
    lv_obj_set_pos(lbl_pol, 60, 220);

    lv_obj_t *pol_pill = lv_obj_create(form);
    lv_obj_remove_style_all(pol_pill);
    lv_obj_set_pos(pol_pill, 440, 208);
    lv_obj_set_size(pol_pill, 292, 46);
    lv_obj_set_style_bg_color(pol_pill, UI_COLOR_CANVAS, 0);
    lv_obj_set_style_bg_opa(pol_pill, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(pol_pill, 10, 0);
    lv_obj_set_style_border_width(pol_pill, 1, 0);
    lv_obj_set_style_border_color(pol_pill, UI_COLOR_OUTLINE, 0);
    lv_obj_clear_flag(pol_pill, LV_OBJ_FLAG_SCROLLABLE);

    const char *pols[2] = { "Active high", "Active low" };
    for (uintptr_t i = 0; i < 2; i++) {
        lv_obj_t *b = lv_btn_create(pol_pill);
        lv_obj_remove_style_all(b);
        lv_obj_set_pos(b, (lv_coord_t)(i * 146), 0);
        lv_obj_set_size(b, 146, 46);
        lv_obj_set_style_radius(b, 9, 0);
        lv_obj_add_style(b, &ui_style_pressed, LV_STATE_PRESSED);
        lv_obj_add_event_cb(b, on_dialog_polarity_btn_click, LV_EVENT_CLICKED, (void *)i);

        s_dialog_active_low_labels[i] = add_static_label(b, pols[i], &ui_font_18, UI_COLOR_INK);
        lv_obj_center(s_dialog_active_low_labels[i]);
        s_dialog_active_low_btns[i] = b;
    }
    s_dialog_active_low_sw = s_dialog_active_low_btns[1];
    if (s_dialog_edit_active_low) {
        lv_obj_add_state(s_dialog_active_low_sw, LV_STATE_CHECKED);
    }

    update_dialog_device_subview();
    if (s_dialog_btn_x != NULL) {
        lv_obj_set_y(s_dialog_btn_x, 352);
        lv_obj_move_foreground(s_dialog_btn_x);
    }
    if (s_dialog_btn_save_dev != NULL) {
        lv_obj_set_y(s_dialog_btn_save_dev, 352);
        lv_obj_move_foreground(s_dialog_btn_save_dev);
    }
}

static void build_dialog_auto_subview(lv_obj_t *box)
{
    if (box == NULL) return;

    if (s_dialog_subview_device != NULL) {
        if (s_dialog_active_low_sw != NULL) {
            s_dialog_edit_active_low = lv_obj_has_state(s_dialog_active_low_sw, LV_STATE_CHECKED) ? 1u : 0u;
        }
        lv_obj_del(s_dialog_subview_device);
        s_dialog_subview_device = NULL;
        s_dialog_preset_dd = NULL;
        s_dialog_mode_btns[0] = s_dialog_mode_btns[1] = NULL;
        s_dialog_mode_labels[0] = s_dialog_mode_labels[1] = NULL;
        s_dialog_active_low_btns[0] = s_dialog_active_low_btns[1] = NULL;
        s_dialog_active_low_labels[0] = s_dialog_active_low_labels[1] = NULL;
        s_dialog_active_low_sw = NULL;
        s_dialog_btn_limits = NULL;
        s_dialog_mode_helper_lbl = NULL;
        s_dialog_device_title_lbl = NULL;
        s_dialog_device_icon = NULL;
        s_dialog_limits_summary_lbl = NULL;
        s_dialog_limits_chevron = NULL;
    }

    s_dialog_subview_auto = lv_obj_create(box);
    s_last_dialog_valid_state = -1;
    lv_obj_remove_style_all(s_dialog_subview_auto);
    lv_obj_set_pos(s_dialog_subview_auto, 0, 0);
    lv_obj_set_size(s_dialog_subview_auto, 752, 350);
    lv_obj_clear_flag(s_dialog_subview_auto, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    /* Back button at (0, 10, 44, 44) - unboxed chevron, optical center aligned */
    lv_obj_t *btn_back = lv_btn_create(s_dialog_subview_auto);
    lv_obj_remove_style_all(btn_back);
    lv_obj_set_pos(btn_back, 0, 10);
    lv_obj_set_size(btn_back, 44, 44);
    lv_obj_add_style(btn_back, &ui_style_pressed, LV_STATE_PRESSED);
    lv_obj_add_flag(btn_back, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(btn_back, on_dialog_limits_back_click, LV_EVENT_CLICKED, NULL);
    lv_obj_t *ico_back = add_icon(btn_back, &ui_icon_chevron_left_20, UI_COLOR_INK);
    lv_obj_center(ico_back);

    /* Contextual Title: "<Preset name> · Auto thresholds" with ui_font_24 */
    static char s_auto_title_buf[64];
    const char *auto_disp_name = (s_dialog_edit_preset == UI_PRESET_FAN) ? "Ventilation" : s_presets[s_dialog_edit_preset].name;
    snprintf(s_auto_title_buf, sizeof(s_auto_title_buf), "%s · Auto thresholds", auto_disp_name);
    lv_obj_t *lbl_auto_title = add_static_label(s_dialog_subview_auto, s_auto_title_buf, &ui_font_24, UI_COLOR_INK);
    lv_obj_set_pos(lbl_auto_title, 44, 16);

    /* Celadon Sensor Strip: (pos: 0, 52, size: 752, 80) */
    lv_obj_t *strip = lv_obj_create(s_dialog_subview_auto);
    lv_obj_remove_style_all(strip);
    lv_obj_set_pos(strip, 0, 52);
    lv_obj_set_size(strip, 752, 80);
    lv_obj_add_style(strip, &ui_style_strip_celadon, 0);
    lv_obj_clear_flag(strip, LV_OBJ_FLAG_SCROLLABLE);

    /* Sensor / Appliance outline icon (44px) */
    lv_obj_t *sensor_ico = add_icon(strip, get_preset_icon_44(s_dialog_edit_preset), UI_COLOR_INK);
    lv_obj_set_pos(sensor_ico, 16, 18);

    /* Sensor name */
    s_dialog_auto_rule_lbl = add_static_label(strip, "", &ui_font_34, UI_COLOR_INK);
    lv_obj_set_pos(s_dialog_auto_rule_lbl, 72, 18);

    /* Vertical separator inside strip */
    lv_obj_t *strip_sep = lv_obj_create(strip);
    lv_obj_remove_style_all(strip_sep);
    lv_obj_set_pos(strip_sep, 470, 16);
    lv_obj_set_size(strip_sep, 1, 48);
    lv_obj_set_style_bg_color(strip_sep, lv_color_hex(0xBCC9A6), 0);
    lv_obj_set_style_bg_opa(strip_sep, LV_OPA_COVER, 0);

    /* "Current" label */
    lv_obj_t *lbl_cur = add_static_label(strip, "Current", &ui_font_18, UI_COLOR_MUTED);
    lv_obj_set_pos(lbl_cur, 490, 6);

    /* Live reading (Font 34 line height 46: 28+46=74 <= 80, zero clipping) */
    s_dialog_live_lbl = add_static_label(strip, s_dialog_live_buf, &ui_font_34, UI_COLOR_INK);
    lv_obj_set_pos(s_dialog_live_lbl, 490, 28);

    s_dialog_live_unit_lbl = add_static_label(strip, s_dialog_live_unit_buf, &ui_font_18, UI_COLOR_MUTED);
    lv_obj_set_pos(s_dialog_live_unit_lbl, 550, 45);

    /* Two Limit Cards (Turn ON and Turn OFF): (368x148 at 0, 144 and 384, 144) */
    for (uint8_t i = 0; i < 2; i++) {
        lv_obj_t *card = lv_obj_create(s_dialog_subview_auto);
        lv_obj_remove_style_all(card);
        lv_obj_set_pos(card, i ? 384 : 0, 144);
        lv_obj_set_size(card, 368, 148);
        lv_obj_add_style(card, &ui_style_card_porcelain, 0);
        lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
        s_dialog_limit_cards[i] = card;

        lv_obj_t *title = add_static_label(card, i ? "Turn OFF" : "Turn ON", &ui_font_24, UI_COLOR_INK);
        lv_obj_set_pos(title, 20, 12);

        lv_obj_t *op = add_static_label(card, "", &ui_font_18, UI_COLOR_MUTED);
        lv_obj_set_pos(op, 20, 44);

        /* Minus button */
        lv_obj_t *btn_minus = lv_btn_create(card);
        lv_obj_remove_style_all(btn_minus);
        lv_obj_set_pos(btn_minus, 20, 80);
        lv_obj_set_size(btn_minus, 48, 48);
        lv_obj_add_style(btn_minus, &ui_style_stepper_btn, 0);
        lv_obj_add_style(btn_minus, &ui_style_pressed, LV_STATE_PRESSED);
        lv_obj_add_style(btn_minus, &ui_style_action_disabled, LV_STATE_DISABLED);
        uintptr_t act_minus = i * 2 + 1;
        lv_obj_add_event_cb(btn_minus, on_thresh_step_click, LV_EVENT_SHORT_CLICKED, (void *)act_minus);
        lv_obj_add_event_cb(btn_minus, on_thresh_step_click, LV_EVENT_LONG_PRESSED_REPEAT, (void *)act_minus);
        lv_obj_t *ico_minus = add_icon(btn_minus, &ui_icon_minus_24, UI_COLOR_INK);
        lv_obj_center(ico_minus);

        /* Central Value Box */
        lv_obj_t *val_box = lv_obj_create(card);
        lv_obj_remove_style_all(val_box);
        lv_obj_set_pos(val_box, 78, 80);
        lv_obj_set_size(val_box, 212, 48);
        lv_obj_add_style(val_box, &ui_style_field_outlined, 0);
        lv_obj_clear_flag(val_box, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *val_lbl = add_static_label(val_box, i ? s_dialog_off_val_buf : s_dialog_on_val_buf, &ui_font_34, UI_COLOR_INK);
        lv_obj_t *unit_lbl = add_static_label(val_box, i ? s_dialog_off_unit_buf : s_dialog_on_unit_buf, &ui_font_18, UI_COLOR_MUTED);

        /* Plus button */
        lv_obj_t *btn_plus = lv_btn_create(card);
        lv_obj_remove_style_all(btn_plus);
        lv_obj_set_pos(btn_plus, 300, 80);
        lv_obj_set_size(btn_plus, 48, 48);
        lv_obj_add_style(btn_plus, &ui_style_stepper_btn, 0);
        lv_obj_add_style(btn_plus, &ui_style_pressed, LV_STATE_PRESSED);
        lv_obj_add_style(btn_plus, &ui_style_action_disabled, LV_STATE_DISABLED);
        uintptr_t act_plus = i * 2 + 0;
        lv_obj_add_event_cb(btn_plus, on_thresh_step_click, LV_EVENT_SHORT_CLICKED, (void *)act_plus);
        lv_obj_add_event_cb(btn_plus, on_thresh_step_click, LV_EVENT_LONG_PRESSED_REPEAT, (void *)act_plus);
        lv_obj_t *ico_plus = add_icon(btn_plus, &ui_icon_plus_24, UI_COLOR_INK);
        lv_obj_center(ico_plus);

        if (i) {
            s_dialog_off_op_lbl = op;
            s_dialog_off_val_lbl = val_lbl;
            s_dialog_off_unit_lbl = unit_lbl;
            s_dialog_btn_off_minus = btn_minus;
            s_dialog_btn_off_plus = btn_plus;
            s_dialog_off_val_box = val_box;
        } else {
            s_dialog_on_op_lbl = op;
            s_dialog_on_val_lbl = val_lbl;
            s_dialog_on_unit_lbl = unit_lbl;
            s_dialog_btn_on_minus = btn_minus;
            s_dialog_btn_on_plus = btn_plus;
            s_dialog_on_val_box = val_box;
        }
    }

    /* Dedicated stable helper / error slot (pos: 0, 300, size: 752, 52) */
    lv_obj_t *helper_slot = lv_obj_create(s_dialog_subview_auto);
    lv_obj_remove_style_all(helper_slot);
    lv_obj_set_pos(helper_slot, 0, 300);
    lv_obj_set_size(helper_slot, 752, 52);
    lv_obj_clear_flag(helper_slot, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    s_dialog_warn_ico = add_icon(helper_slot, &ui_icon_warning_24, UI_COLOR_ERROR);
    lv_obj_set_pos(s_dialog_warn_ico, 8, 4);
    lv_obj_add_flag(s_dialog_warn_ico, LV_OBJ_FLAG_HIDDEN);

    s_dialog_validation_lbl = add_static_label(helper_slot, s_dialog_validation_buf, &ui_font_18, UI_COLOR_MUTED);
    lv_obj_set_pos(s_dialog_validation_lbl, 8, 4);
    lv_obj_set_size(s_dialog_validation_lbl, 736, 46);
    lv_label_set_long_mode(s_dialog_validation_lbl, LV_LABEL_LONG_WRAP);

    /* Reset limits button in footer: pos: 259, 360, size: 234, 48 */
    lv_obj_t *btn_reset = lv_obj_create(box);
    lv_obj_remove_style_all(btn_reset);
    lv_obj_set_pos(btn_reset, 259, 360);
    lv_obj_set_size(btn_reset, 234, 48);
    lv_obj_add_style(btn_reset, &ui_style_btn_outlined, 0);
    lv_obj_add_style(btn_reset, &ui_style_pressed, LV_STATE_PRESSED);
    lv_obj_add_flag(btn_reset, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(btn_reset, on_dialog_reset_limits_click, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_reset = add_static_label(btn_reset, "Reset limits", &ui_font_24, UI_COLOR_INK);
    lv_obj_center(lbl_reset);
    s_dialog_btn_reset = btn_reset;

    update_dialog_auto_subview();

    if (s_dialog_btn_x != NULL) {
        lv_obj_set_y(s_dialog_btn_x, 360);
        lv_obj_move_foreground(s_dialog_btn_x);
    }
    if (s_dialog_btn_save_auto != NULL) {
        lv_obj_set_y(s_dialog_btn_save_auto, 360);
        lv_obj_move_foreground(s_dialog_btn_save_auto);
    }
}

static void open_device_dialog(uint8_t device_idx)
{
    if (device_idx >= UI_DEVICE_COUNT) return;
    if (s_dialog_overlay != NULL) close_device_dialog();

    s_dialog_device_idx = (int8_t)device_idx;
    const ui_hub_device_config_t *cfg = &s_dev_configs[device_idx];

    s_dialog_edit_preset = cfg->preset;
    s_dialog_edit_mode = cfg->mode;
    s_dialog_edit_active_low = cfg->active_low;
    s_dialog_edit_on_thresh = cfg->auto_on_thresh;
    s_dialog_edit_off_thresh = cfg->auto_off_thresh;

    ui_dismiss_toast();

    /* Focused editor replaces the page object tree, not just its pixels.
     * Keep all model/Auto state alive; avoid retaining two screens in 42 KiB. */
    if (s_page_container != NULL) {
        lv_obj_add_flag(s_page_container, LV_OBJ_FLAG_HIDDEN);
        lv_obj_del(s_page_container);
        s_page_container = NULL;
        reset_page_widget_pointers();
    }

    /* Modal overlay */
    s_dialog_overlay = lv_obj_create(s_screen);
    lv_obj_remove_style_all(s_dialog_overlay);
    lv_obj_set_size(s_dialog_overlay, 800, 424);
    lv_obj_set_pos(s_dialog_overlay, 0, 56);
    lv_obj_clear_flag(s_dialog_overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_style(s_dialog_overlay, &ui_style_dialog_overlay, 0);

    /* Hide top nav capsule and content area while dialog is open */
    if (s_nav_bar != NULL) {
        lv_obj_add_flag(s_nav_bar, LV_OBJ_FLAG_HIDDEN);
    }
    if (s_content_area != NULL) {
        lv_obj_add_flag(s_content_area, LV_OBJ_FLAG_HIDDEN);
    }

    /* Focused editor below the persistent header, 24px outer gutters. */
    lv_obj_t *box = lv_obj_create(s_dialog_overlay);
    lv_obj_remove_style_all(box);
    lv_obj_set_size(box, 752, 424);
    lv_obj_set_pos(box, 24, 0);
    lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);
    s_dialog_box = box;

    /* Shared footer Cancel (pos: 0, 352, size: 234, 48) */
    lv_obj_t *btn_x = lv_obj_create(box);
    lv_obj_remove_style_all(btn_x);
    lv_obj_set_pos(btn_x, 0, 352);
    lv_obj_set_size(btn_x, 234, 48);
    lv_obj_add_style(btn_x, &ui_style_btn_outlined, 0);
    lv_obj_add_style(btn_x, &ui_style_pressed, LV_STATE_PRESSED);
    lv_obj_add_flag(btn_x, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(btn_x, on_dialog_close_click, LV_EVENT_CLICKED, NULL);
    lv_obj_t *ic_x = add_static_label(btn_x, "Cancel", &ui_font_24, UI_COLOR_INK);
    lv_obj_center(ic_x);
    s_dialog_btn_x = btn_x;

    /* One Save commits the entire draft from either subview (pos: 518, 352, size: 234, 48) */
    lv_obj_t *btn_save = lv_btn_create(box);
    lv_obj_remove_style_all(btn_save);
    lv_obj_set_pos(btn_save, 518, 352);
    lv_obj_set_size(btn_save, 234, 48);
    lv_obj_add_style(btn_save, &ui_style_btn_forest, 0);
    lv_obj_add_style(btn_save, &ui_style_pressed, LV_STATE_PRESSED);
    lv_obj_add_style(btn_save, &ui_style_action_disabled, LV_STATE_DISABLED);
    lv_obj_add_event_cb(btn_save, on_dialog_save_click, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_save = add_static_label(btn_save, "Save settings", &ui_font_24, lv_color_white());
    lv_obj_center(lbl_save);
    s_dialog_btn_save_dev = btn_save;
    s_dialog_btn_save_auto = btn_save;

    /* Build initial subview 1: Device configuration */
    build_dialog_device_subview(box);
    lv_obj_move_foreground(btn_save);
}

static void close_device_dialog(void)
{
    cleanup_dialog_dd_scrim();
    if (s_dialog_preset_dd != NULL && lv_dropdown_is_open(s_dialog_preset_dd)) {
        lv_dropdown_close(s_dialog_preset_dd);
    }
    if (s_dialog_btn_reset != NULL) {
        lv_obj_del(s_dialog_btn_reset);
        s_dialog_btn_reset = NULL;
    }
    if (s_dialog_overlay != NULL) {
        lv_obj_del(s_dialog_overlay);
        s_dialog_overlay = NULL;
    }
    if (s_nav_bar != NULL) {
        lv_obj_clear_flag(s_nav_bar, LV_OBJ_FLAG_HIDDEN);
    }
    if (s_content_area != NULL) {
        lv_obj_clear_flag(s_content_area, LV_OBJ_FLAG_HIDDEN);
    }
    s_dialog_btn_x = NULL;
    s_dialog_box = NULL;
    s_dialog_device_idx = -1;
    s_dialog_subview_device = NULL;
    s_dialog_subview_auto = NULL;
    s_dialog_preset_dd = NULL;
    s_dialog_mode_btns[0] = s_dialog_mode_btns[1] = NULL;
    s_dialog_mode_labels[0] = s_dialog_mode_labels[1] = NULL;
    s_dialog_active_low_btns[0] = s_dialog_active_low_btns[1] = NULL;
    s_dialog_active_low_labels[0] = s_dialog_active_low_labels[1] = NULL;
    s_dialog_active_low_sw = NULL;
    s_dialog_btn_limits = NULL;
    s_dialog_limits_summary_lbl = NULL;
    s_dialog_limits_chevron = NULL;
    s_dialog_mode_helper_lbl = NULL;
    s_dialog_device_title_lbl = NULL;
    s_dialog_device_icon = NULL;
    s_dialog_auto_rule_lbl = NULL;
    s_dialog_live_lbl = NULL;
    s_dialog_live_unit_lbl = NULL;
    s_dialog_limit_cards[0] = s_dialog_limit_cards[1] = NULL;
    s_dialog_on_op_lbl = NULL;
    s_dialog_off_op_lbl = NULL;
    s_dialog_on_val_lbl = NULL;
    s_dialog_off_val_lbl = NULL;
    s_dialog_on_unit_lbl = NULL;
    s_dialog_off_unit_lbl = NULL;
    s_dialog_on_val_box = s_dialog_off_val_box = NULL;
    s_dialog_warn_ico = NULL;
    s_dialog_validation_lbl = NULL;
    s_dialog_btn_save_auto = NULL;
    s_dialog_btn_save_dev = NULL;
    s_dialog_btn_on_minus = NULL;
    s_dialog_btn_on_plus = NULL;
    s_dialog_btn_off_minus = NULL;
    s_dialog_btn_off_plus = NULL;
    if (s_screen != NULL && s_page_container == NULL) do_navigate_to_page(s_current_page);
}


/* Reset all per-page cached widget pointers to prevent stale references (P2.10) */
static void reset_page_widget_pointers(void)
{
    s_home_air_status = NULL;
    s_home_air_badge = NULL;
    s_home_air_sub_lbl = NULL;
    s_home_co2_val = NULL;
    s_home_co2_unit_lbl = NULL;

    s_home_voc_val = NULL;
    s_home_voc_unit_lbl = NULL;

    s_home_temp_val = NULL;
    s_home_temp_unit_lbl = NULL;
    s_home_temp_note = NULL;
    s_home_humid_val = NULL;
    s_home_humid_unit_lbl = NULL;
    s_home_humid_note = NULL;

    for (uint8_t i = 0; i < UI_DEVICE_COUNT; i++) {
        s_home_dev_icons[i] = NULL;
        s_home_dev_names[i] = NULL;
        s_home_dev_states[i] = NULL;
        s_home_dev_auto_icons[i] = NULL;
        s_home_dev_modes[i] = NULL;
        s_home_dev_dashes[i] = NULL;
        s_home_dev_switches[i] = NULL;
        s_home_dev_retries[i] = NULL;

        s_dev_page_icons[i] = NULL;
        s_dev_page_names[i] = NULL;
        s_dev_page_auto_icons[i] = NULL;
        s_dev_page_modes[i] = NULL;
        s_dev_page_states[i] = NULL;
        s_dev_page_dashes[i] = NULL;
        s_dev_page_switches[i] = NULL;
        s_dev_page_retries[i] = NULL;
    }

    s_trends_title_lbl = NULL;
    s_trends_unit_lbl = NULL;
    s_trends_unavail_badge = NULL;
    s_trends_chart = NULL;
    s_trends_chart_series = NULL;
    for (int i = 0; i < 3; i++) {
        s_trends_y_lbls[i] = NULL;
    }
    s_trends_empty_box = NULL;
    s_trends_empty_lbl = NULL;
    s_trends_empty_hint = NULL;
    for (int i = 0; i < 4; i++) {
        s_trends_tab_btns[i] = NULL;
        s_trends_tab_cards[i] = NULL;
        s_trends_tab_icons[i] = NULL;
        s_trends_tab_names[i] = NULL;
        s_trends_tab_dots[i] = NULL;
        s_trends_tab_vals[i] = NULL;
        s_trends_tab_units[i] = NULL;
        s_trends_time_labels[i] = NULL;
    }
    s_pending_trend_chart = false;
}

/* =========================================================================
 * Page Switcher & Container Lifecycle (P1.3 Async Navigation)
 * ========================================================================= */
static void do_navigate_to_page(ui_page_id_t page)
{
    if (s_dialog_overlay != NULL) close_device_dialog();

    if (page == s_current_page && s_page_container != NULL) return;

    s_pending_trend_chart = false;
    s_stage1_flush_pending = false;
    s_current_page = page;
    update_navigation_selection();

    /* Dismiss open modal dialog if navigating across pages */
    if (s_dialog_overlay != NULL) {
        close_device_dialog();
    }

    /* Delete previous page container and reset cached pointers (P2.10) */
    if (s_page_container != NULL) {
        lv_obj_del(s_page_container);
        s_page_container = NULL;
    }
    reset_page_widget_pointers();

    s_page_container = lv_obj_create(s_content_area);
    lv_obj_remove_style_all(s_page_container);
    lv_obj_set_size(s_page_container, 752, 344);
    lv_obj_clear_flag(s_page_container, LV_OBJ_FLAG_SCROLLABLE);

    switch (page) {
        case UI_PAGE_HOME:
            build_home_page();
            break;
        case UI_PAGE_TRENDS:
            build_trends_page();
            break;
        case UI_PAGE_DEVICES:
            build_devices_page();
            break;
    }

    /* P1.6: Removed lv_obj_invalidate(s_screen). Object creation and deletion
     * natively invalidate dirty areas without 48 redundant render-flush cycles. */
}

static void async_nav_cb(void *param)
{
    ui_page_id_t page = (ui_page_id_t)(uintptr_t)param;
    do_navigate_to_page(page);
}

void ui_navigate_to_page(ui_page_id_t page)
{
    lv_async_call(async_nav_cb, (void *)(uintptr_t)page);
}

void ui_select_trend_metric(ui_metric_id_t metric)
{
    if (metric >= UI_METRIC_COUNT) return;
    if (s_current_page == UI_PAGE_TRENDS && s_selected_metric == metric && !s_pending_trend_chart) {
        /* Already selected on Trends page and chart not pending: strictly a visual & memory no-op (R1) */
        return;
    }
    s_selected_metric = metric;
    s_pending_trend_metric = metric;
    s_pending_trend_chart = false;
    s_stage1_flush_pending = false;
    if (s_current_page == UI_PAGE_TRENDS && s_page_container != NULL) {
        update_trends_tabs();
        update_trends_page_widgets();
    } else {
        ui_navigate_to_page(UI_PAGE_TRENDS);
    }
}

/* =========================================================================
 * Hub-Local Device Configuration & History Ingestion
 * ========================================================================= */
const ui_hub_device_config_t *ui_get_device_config(uint8_t idx)
{
    if (idx >= UI_DEVICE_COUNT) return NULL;
    return &s_dev_configs[idx];
}

#if defined(UI_TEST_HOOKS)
static bool s_test_boot_staging = false;
void ui_test_set_boot_staging(bool active)
{
    s_test_boot_staging = active;
}

void ui_test_reset_device_configs(void)
{
    for (uint8_t i = 0; i < UI_DEVICE_COUNT; i++) {
        s_dev_configured[i] = false;
        ui_auto_get_defaults(i, (i == 3) ? UI_PRESET_LIGHT : (uint8_t)i, &s_dev_configs[i]);
        if (i == 3) {
            s_dev_configs[3].mode = UI_MODE_MANUAL;
        }
    }
}
#endif

bool ui_set_device_config(uint8_t idx, const ui_hub_device_config_t *cfg)
{
    bool is_boot = (s_screen == NULL);
#if defined(UI_TEST_HOOKS)
    if (s_test_boot_staging) is_boot = true;
#endif
    if (cfg != NULL && cfg->schema_version == 0x02u) {
        ui_hub_device_config_t migrated = *cfg;
        migrated.schema_version = UI_CONFIG_SCHEMA_VERSION;
        /* Old VOC thresholds were an index; there is no valid index-to-ppb conversion. */
        if (migrated.preset == UI_PRESET_PURIFIER) {
            migrated.auto_on_thresh = UI_AUTO_VOC_DEF_ON;
            migrated.auto_off_thresh = UI_AUTO_VOC_DEF_OFF;
        }
        return commit_device_config_internal(idx, &migrated, is_boot, NULL, 0);
    }
    return commit_device_config_internal(idx, cfg, is_boot, NULL, 0);
}

void ui_set_config_changed_cb(ui_config_changed_cb_t cb)
{
    s_config_changed_cb = cb;
}

void ui_feed_history(ui_metric_id_t metric, const int32_t *values, const bool *valid, uint16_t count, uint16_t start_minute)
{
    if (metric >= UI_METRIC_COUNT) return;

    /* Mark metric as externally fed; takes exclusive precedence until session reset */
    bool was_external = s_history_externally_fed[metric];
    s_history_externally_fed[metric] = true;

    /* Keep newest window (P1.8) */
    uint16_t offset = (count > UI_HISTORY_CAPACITY) ? (count - UI_HISTORY_CAPACITY) : 0u;
    uint16_t n = count - offset;

    /* Check if refeeding identical history */
    bool changed = !was_external;
    if (!changed) {
        if (s_history_count[metric] != n) {
            changed = true;
        } else {
            for (uint16_t i = 0; i < n; i++) {
                int32_t v = (values != NULL) ? values[offset + i] : 0;
                bool val_valid = (valid != NULL) ? valid[offset + i] : true;
                uint16_t m = (start_minute + offset + i) % 1440u;
                if (s_history[metric][i].value != v ||
                    s_history[metric][i].valid != val_valid ||
                    s_history[metric][i].minute_of_day != m) {
                    changed = true;
                    break;
                }
            }
        }
    }
    if (!changed) {
        return; /* Strict no-op (A4) */
    }

    for (uint16_t i = 0; i < n; i++) {
        s_history[metric][i].value = (values != NULL) ? values[offset + i] : 0;
        s_history[metric][i].valid = (valid != NULL) ? valid[offset + i] : true;
        s_history[metric][i].minute_of_day = (start_minute + offset + i) % 1440u;
    }
    s_history_count[metric] = n;
    s_history_rev[metric]++;

    if (s_current_page == UI_PAGE_TRENDS) {
        update_trends_page_widgets();
    }
}

/* =========================================================================
 * Splash Screen Animation (2.5s, P1.11, P1.12)
 * ========================================================================= */
static void splash_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    uint32_t now = lv_tick_get();
    uint32_t elapsed = now - s_splash_start_tick;

    if (elapsed < 250u) {
        /* Blank screen */
        if (s_splash_phase != 1) {
            s_splash_phase = 1;
            if (s_splash_logo != NULL) lv_obj_set_style_img_opa(s_splash_logo, LV_OPA_TRANSP, 0);
        }
    } else if (elapsed < 600u) {
        /* Fade in & move up 14px */
        s_splash_phase = 2;
        uint32_t t = elapsed - 250u;
        lv_opa_t opa = (lv_opa_t)((t * 255u) / 350u);
        lv_coord_t ofs_y = (lv_coord_t)(14 - (t * 14) / 350u);
        if (s_splash_logo != NULL) {
            lv_obj_set_style_img_opa(s_splash_logo, opa, 0);
            lv_obj_align(s_splash_logo, LV_ALIGN_CENTER, 0, ofs_y);
        }
    } else if (elapsed < 2100u) {
        /* Steady display: set once, do not re-apply every tick */
        if (s_splash_phase != 3) {
            s_splash_phase = 3;
            if (s_splash_logo != NULL) {
                lv_obj_set_style_img_opa(s_splash_logo, LV_OPA_COVER, 0);
                lv_obj_align(s_splash_logo, LV_ALIGN_CENTER, 0, 0);
            }
        }
    } else if (elapsed < 2500u) {
        /* Fade out */
        s_splash_phase = 4;
        uint32_t t = elapsed - 2100u;
        lv_opa_t opa = (lv_opa_t)(255u - (t * 255u) / 400u);
        if (s_splash_logo != NULL) {
            lv_obj_set_style_img_opa(s_splash_logo, opa, 0);
        }
    } else {
        /* Complete: Reveal Main Shell FIRST, then delete splash */
        lv_scr_load(s_screen);
        if (s_splash_screen != NULL) {
            lv_obj_del_async(s_splash_screen);
            s_splash_screen = NULL;
            s_splash_logo = NULL;
        }
        if (s_splash_timer != NULL) {
            lv_timer_del(s_splash_timer);
            s_splash_timer = NULL;
        }
    }
}

void ui_replay_splash(void)
{
    if (s_splash_timer != NULL) {
        lv_timer_del(s_splash_timer);
        s_splash_timer = NULL;
    }
    if (s_splash_screen != NULL) {
        lv_scr_load(s_screen);
        lv_obj_del_async(s_splash_screen);
        s_splash_screen = NULL;
        s_splash_logo = NULL;
    }

    s_splash_screen = lv_obj_create(NULL);
    lv_obj_remove_style_all(s_splash_screen);
    lv_obj_set_size(s_splash_screen, 800, 480);
    /* Synchronized with Quiet Architecture Warm Stone Canvas (0xEEEAE1).
     * ui_img_splash_logo has a baked 0xEEEAE1 background that quantises in RGB565
     * to exact 0xEF5C (238, 234, 230). Setting UI_COLOR_CANVAS ensures identical
     * 0xEF5C quantisation across the screen, eliminating any bounding seam and
     * providing a seamless transition into the canvas. */
    lv_obj_set_style_bg_color(s_splash_screen, UI_COLOR_CANVAS, 0);
    lv_obj_set_style_bg_opa(s_splash_screen, LV_OPA_COVER, 0);

    s_splash_logo = lv_img_create(s_splash_screen);
    lv_img_set_src(s_splash_logo, &ui_img_splash_logo);
    lv_obj_set_style_img_opa(s_splash_logo, LV_OPA_TRANSP, 0);
    lv_obj_align(s_splash_logo, LV_ALIGN_CENTER, 0, 14);

    lv_scr_load(s_splash_screen);
    s_splash_phase = 0;
    s_splash_start_tick = lv_tick_get();
    s_splash_timer = lv_timer_create(splash_timer_cb, 20u, NULL);
}

/* =========================================================================
 * UI Initialization
 * ========================================================================= */
void ui_init(void)
{
    ui_theme_init();

    /* Initialize default Hub-local device configurations only for unconfigured channels */
    for (uint8_t i = 0; i < UI_DEVICE_COUNT; i++) {
        if (!s_dev_configured[i]) {
            ui_auto_get_defaults(i, (i == 3) ? UI_PRESET_LIGHT : (uint8_t)i, &s_dev_configs[i]);
            if (i == 3) {
                s_dev_configs[3].mode = UI_MODE_MANUAL;
            }
        }
        s_desired_on[i] = false;
        s_device_cmds[i].phase = UI_CMD_PHASE_IDLE;
        s_device_cmds[i].request_tick_ms = 0;
        ui_auto_state_init(&s_auto_states[i], 0);
        ui_auto_restart_hold(&s_auto_states[i], (lv_tick_get() > 0) ? lv_tick_get() : 1u);
    }

    /* Initialize history storage and autonomous tracking */
    for (uint8_t m = 0; m < UI_METRIC_COUNT; m++) {
        s_history_externally_fed[m] = false;
        s_auto_hist_started[m] = false;
        s_auto_hist_last_minute[m] = 0;
        s_history_count[m] = 0;
        s_history_rev[m] = 1u;
        s_history_rendered_rev[m] = 0xFFFFFFFFu;
        for (uint16_t p = 0; p < UI_HISTORY_CAPACITY; p++) {
            s_history[m][p].value = 0;
            s_history[m][p].valid = false;
            s_history[m][p].minute_of_day = 0;
        }
    }

    /* Outbound transmit gating: false until first fresh report is reconciled */
    s_tx_ready = false;
    s_last_node_uptime = 0U;
    s_have_node_sample = false;

    /* Initialize default command header */
    lora_hub_cmd_init_header(&lora_hub_cmd);

    /* Main Permanent Screen */
    s_screen = lv_obj_create(NULL);
    lv_obj_remove_style_all(s_screen);
    lv_obj_set_size(s_screen, 800, 480);
    lv_obj_add_style(s_screen, &ui_style_canvas, 0);
    lv_obj_clear_flag(s_screen, LV_OBJ_FLAG_SCROLLABLE);

    /* Permanent Header */
    build_header();

    /* Permanent top navigation row (y=56..100) */
    build_navigation();

    /* Content area below header and navigation: 752x344 at (24, 112) */
    s_content_area = lv_obj_create(s_screen);
    lv_obj_remove_style_all(s_content_area);
    lv_obj_set_pos(s_content_area, 24, 112);
    lv_obj_set_size(s_content_area, 752, 344);
    lv_obj_clear_flag(s_content_area, LV_OBJ_FLAG_SCROLLABLE);

    /* Permanent Toast container */
    build_toast();

    /* Build Initial Page */
    do_navigate_to_page(s_current_page);
    update_header();
    update_uptime_string();

    s_last_tick_ms = lv_tick_get();

    /* Load Main Screen */
    lv_scr_load(s_screen);
}

bool ui_is_tx_ready(void)
{
    return s_tx_ready;
}

bool ui_has_pending_command(void)
{
    for (uint8_t i = 0; i < UI_DEVICE_COUNT; i++) {
        if (s_device_cmds[i].phase == UI_CMD_PHASE_PENDING) return true;
    }
    return false;
}

/* =========================================================================
 * UI Periodic Tick Hook
 * ========================================================================= */
void ui_tick(void)
{
    uint32_t now = lv_tick_get();
    uint32_t delta = now - s_last_tick_ms; /* Handles 32-bit unsigned rollover */
    s_last_tick_ms = now;

    s_ms_accumulator += delta;
    if (s_ms_accumulator >= 1000u) {
        uint32_t elapsed_secs = s_ms_accumulator / 1000u;
        s_ms_accumulator %= 1000u;
        s_uptime_seconds += elapsed_secs;
    }

    if (s_dialog_dd_scrim != NULL) {
        if (s_dialog_preset_dd == NULL || !lv_dropdown_is_open(s_dialog_preset_dd)) {
            cleanup_dialog_dd_scrim();
        }
    }

    bool online = is_link_connected(now);
    bool rx_changed = (lora_rx_revision != s_last_rendered_rx_revision);
    if (rx_changed && online) {
        if (s_tx_ready && lora_node_status.bits.uptime_sec < s_last_node_uptime) {
            s_tx_ready = false;
            s_last_rendered_link_state = false;
            s_have_node_sample = false;
        }
        s_last_node_uptime = lora_node_status.bits.uptime_sec;
    }
    bool link_state_changed = (online != s_last_rendered_link_state);
    bool cmd_state_changed = false;
    bool any_timed_out = false;

    /* Handle link transition or startup synchronization */
    if (online) {
        if (!s_tx_ready || link_state_changed) {
            /* Startup sync or Reconnect: adopt all four reported GPIO levels atomically */
            s_tx_ready = true;
            for (uint8_t i = 0; i < UI_DEVICE_COUNT; i++) {
                uint8_t rep_gpio = lora_node_status_get_relay_gpio(&lora_node_status, i);
                s_desired_on[i] = (rep_gpio ^ s_dev_configs[i].active_low) != 0;
                lora_hub_cmd_set_relay_gpio(&lora_hub_cmd, i, rep_gpio);
                s_device_cmds[i].phase = UI_CMD_PHASE_IDLE;
                ui_auto_restart_hold(&s_auto_states[i], now);
                ui_auto_reset_qualification(&s_auto_states[i]);
            }
            cmd_state_changed = true;
        }
    } else {
        if (s_tx_ready || link_state_changed) {
            /* Disconnect: invalidate abandoned command tracking and gate outbound TX */
            s_tx_ready = false;
            s_have_node_sample = false;
            for (uint8_t i = 0; i < UI_DEVICE_COUNT; i++) {
                if (s_device_cmds[i].phase == UI_CMD_PHASE_PENDING) {
                    s_device_cmds[i].phase = UI_CMD_PHASE_IDLE;
                }
                ui_auto_reset_qualification(&s_auto_states[i]);
            }
            cmd_state_changed = true;
        }
    }

    /* Sweep relay commands: desired versus reported */
    for (uint8_t i = 0; i < UI_DEVICE_COUNT; i++) {
        ui_device_cmd_state_t *cmd = &s_device_cmds[i];
        uint8_t reported_gpio = lora_node_status_get_relay_gpio(&lora_node_status, i);
        bool reported_on = (reported_gpio ^ s_dev_configs[i].active_low) != 0;

        if (cmd->phase == UI_CMD_PHASE_PENDING) {
            bool seq_ok = (int16_t)(lora_node_status.bits.seq_echo - cmd->seq) >= 0;
            if (online && reported_on == s_desired_on[i] && seq_ok) {
                cmd->phase = UI_CMD_PHASE_IDLE;
                cmd_state_changed = true;
                /* Restart hold on confirmed transition */
                ui_auto_restart_hold(&s_auto_states[i], now);
            } else if ((now - cmd->request_tick_ms) >= UI_CMD_TIMEOUT_MS) {
                cmd->phase = UI_CMD_PHASE_ERROR;
                lora_hub_cmd_set_relay_gpio(&lora_hub_cmd, i, reported_gpio);
                any_timed_out = true;
                cmd_state_changed = true;
                /* Auto timeout latches paused error */
                if (s_dev_configs[i].mode == UI_MODE_AUTO) {
                    ui_auto_pause(&s_auto_states[i]);
                }
            }
        } else if (cmd->phase == UI_CMD_PHASE_ERROR) {
            if (online && reported_on == s_desired_on[i]) {
                /* Late report arrived after timeout */
                cmd->phase = UI_CMD_PHASE_IDLE;
                lora_hub_cmd_set_relay_gpio(&lora_hub_cmd, i, reported_gpio);
                cmd_state_changed = true;
                /* Late report does NOT clear s_auto_states[i].paused_error */
            }
        } else {
            /* Settled idle (A1): ensure command wire level matches reported GPIO even when logical states agree */
            uint8_t cmd_gpio = lora_hub_cmd_get_relay_gpio(&lora_hub_cmd, i);
            if (online && (s_desired_on[i] != reported_on || cmd_gpio != reported_gpio)) {
                s_desired_on[i] = reported_on;
                lora_hub_cmd_set_relay_gpio(&lora_hub_cmd, i, reported_gpio);
                cmd_state_changed = true;
                ui_auto_restart_hold(&s_auto_states[i], now);
                ui_auto_reset_qualification(&s_auto_states[i]);
            }
        }
    }
    if (any_timed_out) {
        ui_announce("No response. Try Retry.");
    } else {
        bool any_pending = false;
        for (uint8_t i = 0; i < UI_DEVICE_COUNT; i++) {
            if (s_device_cmds[i].phase == UI_CMD_PHASE_PENDING) {
                any_pending = true;
                break;
            }
        }
        if (!any_pending && s_toast_obj != NULL && !lv_obj_has_flag(s_toast_obj, LV_OBJ_FLAG_HIDDEN)) {
            if (strcmp(s_toast_buf, "Sending…") == 0 || strcmp(s_toast_buf, "Retrying…") == 0) {
                ui_dismiss_toast();
            }
        }
    }

    /* Evaluate Auto rules strictly on fresh received frames when online and tx_ready */
    bool new_sample = rx_changed && online && (!s_have_node_sample ||
                       lora_node_status.bits.sample_seq != s_last_auto_sample_seq);
    if (new_sample) {
        s_last_auto_sample_seq = lora_node_status.bits.sample_seq;
        s_have_node_sample = true;
    }
    if (new_sample && online && s_tx_ready) {
        for (uint8_t i = 0; i < UI_DEVICE_COUNT; i++) {
            if (s_dev_configs[i].mode == UI_MODE_AUTO && s_device_cmds[i].phase == UI_CMD_PHASE_IDLE) {
                uint8_t reported_gpio = lora_node_status_get_relay_gpio(&lora_node_status, i);
                bool reported_on = (reported_gpio ^ s_dev_configs[i].active_low) != 0;
                bool target_on = false;
                if (ui_auto_eval(&s_auto_states[i], &s_dev_configs[i], &lora_node_status, online, reported_on, now, &target_on)) {
                    s_desired_on[i] = target_on;
                    uint8_t desired_gpio = (target_on ^ s_dev_configs[i].active_low) ? 1u : 0u;
                    lora_hub_cmd_set_relay_gpio(&lora_hub_cmd, i, desired_gpio);
                    lora_hub_cmd.bits.seq++;
                    s_device_cmds[i].phase = UI_CMD_PHASE_PENDING;
                    s_device_cmds[i].request_tick_ms = now;
                    s_device_cmds[i].seq = lora_hub_cmd.bits.seq;
                    cmd_state_changed = true;
                }
            }
        }

        /* Autonomous bounded session history sampling (P1) */
        uint32_t session_min = s_uptime_seconds / 60u;
        for (uint8_t m = 0; m < UI_METRIC_COUNT; m++) {
            if (s_history_externally_fed[m]) continue;
            bool avail = ui_is_sensor_available((ui_metric_id_t)m, now);
            int32_t val = get_live_metric_raw_value((ui_metric_id_t)m);
            if (!s_auto_hist_started[m]) {
                s_auto_hist_started[m] = true;
                s_auto_hist_last_minute[m] = session_min;
                s_history[m][0].minute_of_day = session_min;
                s_history[m][0].valid = avail;
                s_history[m][0].value = avail ? val : 0;
                s_history_count[m] = 1;
                s_history_rev[m]++;
            } else {
                if (session_min > s_auto_hist_last_minute[m]) {
                    uint32_t diff = session_min - s_auto_hist_last_minute[m];
                    uint32_t gap_start = (diff >= UI_HISTORY_CAPACITY && session_min >= (UI_HISTORY_CAPACITY - 1))
                                         ? (session_min - (UI_HISTORY_CAPACITY - 1))
                                         : (s_auto_hist_last_minute[m] + 1);
                    for (uint32_t k = gap_start; k < session_min; k++) {
                        push_history_sample((ui_metric_id_t)m, k, false, 0);
                    }
                    push_history_sample((ui_metric_id_t)m, session_min, avail, avail ? val : 0);
                    s_auto_hist_last_minute[m] = session_min;
                    s_history_rev[m]++;
                } else {
                    /* Same session minute: the newest slot follows the latest valid reading so the
                     * chart ("Now") and its range track live values; a closed minute keeps its last
                     * reading. Unavailable readings never erase a minute that already has data, and
                     * an unchanged value leaves the revision alone (0-flush idle). */
                    uint16_t last_idx = (s_history_count[m] > 0) ? (s_history_count[m] - 1) : 0;
                    ui_history_sample_t *last = &s_history[m][last_idx];
                    if (s_history_count[m] > 0 && avail && (!last->valid || last->value != val)) {
                        last->valid = true;
                        last->value = val;
                        s_history_rev[m]++;
                    }
                }
            }
        }
    }

    /* Stage 2: Deferred Trends heavy chart rendering (coalesced latest tap wins).
     * Taps closer together than UI_TRENDS_SETTLE_MS only move the tab highlight;
     * the card is rendered once for the metric the user settles on. */
    bool trends_updated = false;
    if (s_pending_trend_chart && s_current_page == UI_PAGE_TRENDS) {
        if (lv_tick_elaps(s_pending_trend_tick) < UI_TRENDS_SETTLE_MS) {
            goto skip_stage2;
        }
        if (s_stage1_flush_pending) {
            if (ui_is_disp_busy_or_dirty()) {
                goto skip_stage2;
            }
            s_stage1_flush_pending = false;
        }
        s_pending_trend_chart = false;
        s_selected_metric = s_pending_trend_metric;
        update_trends_tabs();
        update_trends_page_widgets();
        trends_updated = true;
        lv_disp_t *disp = lv_disp_get_default();
        if (disp && disp->refr_timer) {
            lv_timer_ready(disp->refr_timer);
        }
    }
skip_stage2:

    /* Cheap change test: only refresh widgets when link state changed, commands changed, or new frame received */
    if (rx_changed || link_state_changed || cmd_state_changed) {
        s_last_rendered_rx_revision = lora_rx_revision;
        s_last_rendered_link_state = online;

        update_header();
        update_dialog_live_reading();
        if (s_current_page == UI_PAGE_HOME) {
            update_home_page_widgets();
        } else if (s_current_page == UI_PAGE_TRENDS && !s_pending_trend_chart && !trends_updated) {
            update_trends_page_widgets();
        } else if (s_current_page == UI_PAGE_DEVICES) {
            update_devices_page_widgets();
        }
    }
}

/* =========================================================================
 * Test Interface Hooks (Sim-only under UI_TEST_HOOKS, excluded from MCU export)
 * ========================================================================= */
#if defined(UI_TEST_HOOKS)
static lv_obj_t *s_null_widget = NULL;

static void reset_history_hook(void)
{
    for (uint8_t m = 0; m < UI_METRIC_COUNT; m++) {
        s_history_externally_fed[m] = false;
        s_auto_hist_started[m] = false;
        s_auto_hist_last_minute[m] = 0;
        s_history_count[m] = 0;
        s_history_rev[m]++;
        s_history_rendered_rev[m] = 0xFFFFFFFFu;
        for (uint16_t p = 0; p < UI_HISTORY_CAPACITY; p++) {
            s_history[m][p].value = 0;
            s_history[m][p].valid = false;
            s_history[m][p].minute_of_day = 0;
        }
    }
    if (s_current_page == UI_PAGE_TRENDS) {
        update_trends_page_widgets();
    }
}

static void trigger_metric_tab_click_hook(ui_metric_id_t metric)
{
    if (metric >= UI_METRIC_COUNT) return;
    if (s_trends_tab_cards[metric] != NULL) {
        lv_event_send(s_trends_tab_cards[metric], LV_EVENT_PRESSED, NULL);
    }
}

const ui_test_hooks_t *ui_get_test_hooks(void)
{
    static const ui_test_hooks_t hooks = {
        .dev_configs = s_dev_configs,
        .desired_on = s_desired_on,
        .device_cmds = s_device_cmds,
        .presets = s_presets,
        .uptime_seconds = &s_uptime_seconds,
        .update_uptime_string = update_uptime_string,
        .format_temp_c = format_temp_c,
        .toggle_device = toggle_device,
        .open_device_dialog = open_device_dialog,
        .close_device_dialog = close_device_dialog,
        .on_dialog_save_click = on_dialog_save_click,

        .history = s_history,
        .history_count = s_history_count,
        .selected_metric = &s_selected_metric,
        .history_rev = s_history_rev,
        .history_externally_fed = s_history_externally_fed,
        .pending_trend_chart = &s_pending_trend_chart,
        .stage1_flush_pending = &s_stage1_flush_pending,
        .pending_trend_metric = &s_pending_trend_metric,
        .trigger_metric_tab_click = trigger_metric_tab_click_hook,
        .trends_title_lbl = &s_trends_title_lbl,

        .co2_val_buf = s_co2_val_buf,
        .voc_val_buf = s_voc_val_buf,
        .temp_val_buf = s_temp_val_buf,
        .humid_val_buf = s_humid_val_buf,
        .co2_badge_buf = s_co2_badge_buf,
        .voc_badge_buf = s_voc_badge_buf,
        .dev_name_buf = s_dev_name_buf,
        .dev_state_buf = s_dev_state_buf,
        .dev_mode_buf = s_dev_mode_buf,
        .trends_y_buf = s_trends_y_buf,
        .time_axis_buf = s_time_axis_buf,
        .trends_tab_val_buf = s_trends_tab_val_buf,
        .trends_tab_unit_buf = s_trends_tab_unit_buf,

        .trends_chart = &s_trends_chart,
        .trends_chart_series = &s_trends_chart_series,
        .trends_unit_lbl = &s_trends_unit_lbl,
        .trends_unavail_badge = &s_trends_unavail_badge,
        .trends_empty_box = &s_trends_empty_box,
        .trends_empty_lbl = &s_trends_empty_lbl,
        .trends_empty_hint = &s_trends_empty_hint,
        .trends_y_lbls = s_trends_y_lbls,
        .trends_tab_cards = s_trends_tab_cards,
        .trends_tab_icons = s_trends_tab_icons,
        .trends_tab_names = s_trends_tab_names,
        .trends_tab_dots = s_trends_tab_dots,
        .trends_tab_vals = s_trends_tab_vals,
        .trends_tab_units = s_trends_tab_units,
        .trends_time_labels = s_trends_time_labels,

        .home_co2_val = &s_home_co2_val,
        .dev_page_names = s_dev_page_names,
        .dev_page_modes = s_dev_page_modes,
        .dev_page_states = s_dev_page_states,
        .dev_page_switches = s_dev_page_switches,
        .dev_page_retries = s_dev_page_retries,
        .dev_page_dashes = s_dev_page_dashes,
        .home_dev_names = s_home_dev_names,
        .home_dev_modes = s_home_dev_modes,
        .home_dev_states = s_home_dev_states,
        .home_dev_switches = s_home_dev_switches,
        .home_dev_retries = s_home_dev_retries,
        .home_dev_dashes = s_home_dev_dashes,
        .dialog_overlay = &s_dialog_overlay,
        .dialog_box = &s_dialog_box,
        .dialog_btn_x = &s_dialog_btn_x,
        .dialog_preset_dd = &s_dialog_preset_dd,
        .dialog_edit_preset = &s_dialog_edit_preset,
        .dialog_edit_mode = &s_dialog_edit_mode,
        .dialog_edit_active_low = &s_dialog_edit_active_low,
        .dialog_mode_btns = s_dialog_mode_btns,
        .dialog_mode_labels = s_dialog_mode_labels,
        .dialog_btn_modes = s_dialog_btn_modes,
        .dialog_active_low_sw = &s_dialog_active_low_sw,

        /* Auto v1 & Threshold Editor hooks */
        .auto_states = s_auto_states,
        .dialog_edit_on_thresh = &s_dialog_edit_on_thresh,
        .dialog_edit_off_thresh = &s_dialog_edit_off_thresh,
        .dialog_validation_buf = s_dialog_validation_buf,
        .dialog_subview_device = &s_dialog_subview_device,
        .dialog_subview_auto = &s_dialog_subview_auto,
        .dialog_btn_limits = &s_dialog_btn_limits,
        .dialog_btn_save_auto = &s_dialog_btn_save_auto,
        .dialog_btn_on_minus = &s_dialog_btn_on_minus,
        .dialog_btn_on_plus = &s_dialog_btn_on_plus,
        .dialog_btn_off_minus = &s_dialog_btn_off_minus,
        .dialog_btn_off_plus = &s_dialog_btn_off_plus,
        .step_threshold = step_threshold,
        .on_dialog_reset_limits_click = on_dialog_reset_limits_click,
        .on_dialog_limits_back_click = on_dialog_limits_back_click,
        .on_dialog_open_limits_click = on_dialog_open_limits_click,

        .link_label = &s_link_label,
        .link_bars_cont = &s_link_bars_cont,
        .header_sep = &s_null_widget,
        .uptime_label = &s_null_widget,
        .header_clock_ico = &s_null_widget,

        .home_temp_note = &s_home_temp_note,
        .home_humid_note = &s_home_humid_note,
        .home_air_status = &s_home_air_status,
        .dialog_live_lbl = &s_dialog_live_lbl,
        .reset_history = reset_history_hook,
    };
    return &hooks;
}

bool ui_test_is_link_connected(uint32_t now)
{
    return is_link_connected(now);
}
#endif
