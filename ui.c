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
 * Shell and Global Object Pointers
 * ========================================================================= */
static lv_obj_t *s_screen;
static lv_obj_t *s_header;
static lv_obj_t *s_content_area;
static lv_obj_t *s_nav_bar;
static lv_obj_t *s_nav_btns[3];
static lv_obj_t *s_nav_icons[3];

/* Header widgets */
static lv_obj_t *s_uptime_label;
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

static lv_obj_t *s_dialog_mode_btns[2] = {NULL, NULL};
static lv_obj_t *s_dialog_mode_labels[2] = {NULL, NULL};
static lv_obj_t *s_dialog_preset_dd = NULL;
static lv_obj_t *s_dialog_dd_scrim = NULL;
static lv_obj_t *s_dialog_active_low_sw = NULL;
static lv_obj_t *s_dialog_btn_limits = NULL;
static lv_obj_t *s_dialog_mode_helper_lbl = NULL;

/* Subview auto threshold widgets */
static lv_obj_t *s_dialog_auto_rule_lbl = NULL;
static lv_obj_t *s_dialog_on_op_lbl = NULL;
static lv_obj_t *s_dialog_off_op_lbl = NULL;
static lv_obj_t *s_dialog_on_val_lbl = NULL;
static lv_obj_t *s_dialog_off_val_lbl = NULL;
static char s_dialog_auto_rule_buf[64] = "";
static char s_dialog_on_op_buf[32] = "";
static char s_dialog_off_op_buf[32] = "";
static char s_dialog_on_val_buf[16] = "";
static char s_dialog_off_val_buf[16] = "";
static lv_obj_t *s_dialog_validation_lbl = NULL;
static char s_dialog_validation_buf[64] = "";
static lv_obj_t *s_dialog_btn_save_auto = NULL;
static lv_obj_t *s_dialog_btn_save_dev = NULL;

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

/* Revision and link state caching for 0-flush idle polling */
static uint32_t s_last_rendered_rx_revision = 0xFFFFFFFFu;
static bool s_last_rendered_link_state = false;

/* Trends caching for 0-flush idle updates (A4) */
static int32_t s_last_trends_range_min = 0;
static int32_t s_last_trends_range_max = 0;
static uint32_t s_history_rendered_rev[UI_METRIC_COUNT] = {0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu};
static uint32_t s_history_rev[UI_METRIC_COUNT] = {1u, 1u, 1u, 1u};
static ui_metric_id_t s_last_rendered_metric = (ui_metric_id_t)0xFF;
static int16_t s_trends_inspected_point = -1;

static inline bool is_link_connected(uint32_t now)
{
    if (lora_last_rx_tick_ms == 0) return false;
    return ((now - lora_last_rx_tick_ms) < LORA_LINK_TIMEOUT_MS);
}

/* History storage for 4 metrics (16 samples each) */
static ui_history_sample_t s_history[UI_METRIC_COUNT][UI_HISTORY_CAPACITY];
static uint16_t s_history_count[UI_METRIC_COUNT];
static lv_coord_t s_chart_ext_y[UI_HISTORY_CAPACITY];

/* Dedicated process-lifetime static text buffers (P1.5) */
static char s_uptime_buf[16] = "00:00:00";
static char s_co2_val_buf[16] = "—";
static char s_co2_badge_buf[16] = "Unknown";
static char s_co2_note_buf[32] = "Connection lost";
static char s_voc_val_buf[16] = "—";
static char s_voc_badge_buf[16] = "Unknown";
static char s_voc_note_buf[32] = "Connection lost";
static char s_temp_val_buf[24] = "—";
static char s_temp_note_buf[32] = "Data unavailable";
static char s_humid_val_buf[24] = "—";
static char s_humid_note_buf[32] = "Data unavailable";

static char s_dev_name_buf[UI_DEVICE_COUNT][24];
static char s_dev_state_buf[UI_DEVICE_COUNT][24];
static char s_dev_mode_buf[UI_DEVICE_COUNT][16];

static char s_stat_cur_buf[32] = "—";
static char s_stat_min_buf[32] = "—";
static char s_stat_max_buf[32] = "—";
static char s_chart_tooltip_buf[64] = "";
static char s_toast_buf[64] = "";
static char s_time_axis_buf[4][8];

/* Header static widgets for dynamic right-alignment */
static lv_obj_t *s_header_clock_ico = NULL;
static lv_obj_t *s_header_sep = NULL;

/* In-place Home page live widget pointers (P0.1, P1.15) */
static lv_obj_t *s_home_co2_val = NULL;
static lv_obj_t *s_home_co2_ppm_lbl = NULL;
static lv_obj_t *s_home_co2_badge = NULL;
static lv_obj_t *s_home_co2_badge_dot = NULL;
static lv_obj_t *s_home_co2_badge_lbl = NULL;
static lv_obj_t *s_home_co2_note = NULL;

static lv_obj_t *s_home_voc_val = NULL;
static lv_obj_t *s_home_voc_badge = NULL;
static lv_obj_t *s_home_voc_badge_dot = NULL;
static lv_obj_t *s_home_voc_badge_lbl = NULL;
static lv_obj_t *s_home_voc_note = NULL;

static lv_obj_t *s_home_temp_val = NULL;
static lv_obj_t *s_home_temp_note = NULL;
static lv_obj_t *s_home_humid_val = NULL;
static lv_obj_t *s_home_humid_note = NULL;

static lv_obj_t *s_home_dev_icons[UI_DEVICE_COUNT];
static lv_obj_t *s_home_dev_names[UI_DEVICE_COUNT];
static lv_obj_t *s_home_dev_states[UI_DEVICE_COUNT];
static lv_obj_t *s_home_dev_modes[UI_DEVICE_COUNT];
static lv_obj_t *s_home_dev_dashes[UI_DEVICE_COUNT];
static lv_obj_t *s_home_dev_switches[UI_DEVICE_COUNT];
static lv_obj_t *s_home_dev_retries[UI_DEVICE_COUNT];

/* In-place Trends page live widget pointers */
static lv_obj_t *s_trends_cur_lbl = NULL;
static lv_obj_t *s_trends_min_lbl = NULL;
static lv_obj_t *s_trends_max_lbl = NULL;
static lv_obj_t *s_trends_unavail_badge = NULL;
static lv_obj_t *s_trends_chart = NULL;
static lv_chart_series_t *s_trends_chart_series = NULL;
static lv_obj_t *s_trends_y_lbls[3] = { NULL, NULL, NULL };
static char s_trends_y_buf[3][16];
static lv_obj_t *s_trends_tooltip = NULL;
static lv_obj_t *s_trends_empty_box = NULL;
static lv_obj_t *s_trends_tab_btns[4];
static lv_obj_t *s_trends_time_labels[4];

/* In-place Devices page live widget pointers */
static lv_obj_t *s_dev_page_icons[UI_DEVICE_COUNT];
static lv_obj_t *s_dev_page_names[UI_DEVICE_COUNT];
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
static void cleanup_dialog_dd_scrim(void);
static void on_dialog_active_low_change(lv_event_t *e);
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

static const lv_img_dsc_t *get_preset_icon(uint8_t preset)
{
    switch (preset) {
        case UI_PRESET_PURIFIER: return &ui_icon_purifier_24;
        case UI_PRESET_FAN: return &ui_icon_fan_24;
        case UI_PRESET_HUMIDIFIER:
        case UI_PRESET_DEHUMIDIFIER: return &ui_icon_drop_24;
        case UI_PRESET_HEATER: return &ui_icon_heat_24;
        case UI_PRESET_LIGHT: return &ui_icon_light_24;
        case UI_PRESET_PLUG: return &ui_icon_plug_24;
        default: return &ui_icon_devices_24;
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
    lv_obj_set_style_img_recolor(img, color, 0);
    lv_obj_set_style_img_recolor_opa(img, LV_OPA_COVER, 0);
    return img;
}

static void format_temp_c(char *buf, size_t buf_size, int32_t tenths)
{
    int32_t abs_tenths = (tenths < 0) ? -tenths : tenths;
    snprintf(buf, buf_size, "%s%d.%d", (tenths < 0) ? "-" : "",
             (int)(abs_tenths / 10), (int)(abs_tenths % 10));
}

/* Unified metric formatting helper ensuring consistent unit spacing across all views (P3.14) */
static void format_metric_value(char *buf, size_t size, ui_metric_id_t metric, int32_t val, bool valid)
{
    if (!valid) {
        snprintf(buf, size, "—");
        return;
    }
    switch (metric) {
        case UI_METRIC_CO2:
            snprintf(buf, size, "%d ppm", (int)val);
            break;
        case UI_METRIC_VOC:
            snprintf(buf, size, "%d", (int)val);
            break;
        case UI_METRIC_TEMP: {
            char t_str[16];
            format_temp_c(t_str, sizeof(t_str), val);
            snprintf(buf, size, "%s°C", t_str);
            break;
        }
        case UI_METRIC_HUMIDITY:
            snprintf(buf, size, "%d%%", (int)val);
            break;
        default:
            snprintf(buf, size, "%d", (int)val);
            break;
    }
}

/* =========================================================================
 * Change-Guard Helpers (P1.4 Redundant Invalidation Elimination)
 * ========================================================================= */
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

const char *ui_get_uptime_str(void)
{
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

static void update_uptime_string(void)
{
    uint32_t hours = s_uptime_seconds / 3600u;
    uint32_t minutes = (s_uptime_seconds / 60u) % 60u;
    uint32_t seconds = s_uptime_seconds % 60u;
    size_t old_len = strlen(s_uptime_buf);
    char tmp[16];
    snprintf(tmp, sizeof(tmp), "%02u:%02u:%02u",
             (unsigned)hours, (unsigned)minutes, (unsigned)seconds);
    if (set_buffer_and_label_if_changed(s_uptime_label, s_uptime_buf, sizeof(s_uptime_buf), tmp)) {
        /* If the rendered string widened (e.g. 99h -> 100h, 999h -> 1000h),
         * recompute header alignment so the separator gap remains preserved (P3.15). */
        if (strlen(s_uptime_buf) != old_len) {
            update_header();
        }
    }
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
}

void ui_announce(const char *message)
{
    if (message == NULL || s_toast_obj == NULL) return;
    snprintf(s_toast_buf, sizeof(s_toast_buf), "%s", message);
    lv_label_set_text_static(s_toast_label, s_toast_buf);
    lv_obj_clear_flag(s_toast_obj, LV_OBJ_FLAG_HIDDEN);
    lv_obj_align(s_toast_obj, LV_ALIGN_BOTTOM_MID, 0, -74);

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
    lv_obj_t *home_ico = add_icon(s_header, &ui_icon_home_24, UI_COLOR_ICON_BLUE);
    lv_obj_set_pos(home_ico, 20, 16);

    lv_obj_t *studio_lbl = add_static_label(s_header, "Studio", &ui_font_24, UI_COLOR_INK);
    lv_obj_set_pos(studio_lbl, 52, 11);

    /* Right cluster: dynamically aligned to right edge (x=778) in update_header() */
    s_header_clock_ico = add_icon(s_header, &ui_icon_timer_20, UI_COLOR_MUTED);
    s_uptime_label = add_static_label(s_header, s_uptime_buf, &ui_font_18, UI_COLOR_INK);

    s_header_sep = lv_obj_create(s_header);
    lv_obj_remove_style_all(s_header_sep);
    lv_obj_set_size(s_header_sep, 1, 18);
    lv_obj_set_style_bg_color(s_header_sep, UI_COLOR_LINE, 0);
    lv_obj_set_style_bg_opa(s_header_sep, LV_OPA_COVER, 0);

    /* 4 Signal bars indicator container (width 18, height 14) */
    s_link_bars_cont = lv_obj_create(s_header);
    lv_obj_remove_style_all(s_link_bars_cont);
    lv_obj_set_size(s_link_bars_cont, 18, 14);
    lv_obj_clear_flag(s_link_bars_cont, LV_OBJ_FLAG_SCROLLABLE);

    static const lv_coord_t s_bar_heights[4] = {4, 7, 10, 14};
    for (int i = 0; i < 4; i++) {
        s_link_bars[i] = lv_obj_create(s_link_bars_cont);
        lv_obj_remove_style_all(s_link_bars[i]);
        lv_obj_set_size(s_link_bars[i], 3, s_bar_heights[i]);
        lv_obj_set_pos(s_link_bars[i], (lv_coord_t)(i * 5), (lv_coord_t)(14 - s_bar_heights[i]));
        lv_obj_set_style_bg_color(s_link_bars[i], UI_COLOR_SWITCH_OFF, 0);
        lv_obj_set_style_bg_opa(s_link_bars[i], LV_OPA_COVER, 0);
    }
    s_current_link_bars = 0;

    s_link_label = add_static_label(s_header, "Disconnected", &ui_font_18, UI_COLOR_INK);

    update_header();
}

static void update_header(void)
{
    if (s_header == NULL || s_uptime_label == NULL) return;

    static bool s_header_initialized = false;
    bool up_changed = update_label_if_changed(s_uptime_label, s_uptime_buf);
    bool link_changed = false;

    uint8_t target_bars = 0;
    uint32_t now = lv_tick_get();
    if (is_link_connected(now)) {
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

    if (!s_header_initialized || up_changed || link_changed) {
        s_header_initialized = true;
        /* Right-align cluster ending at x=778 (22px padding from 800px edge).
         * Single full-screen layout update resolves both label widths (P1.5). */
        lv_obj_update_layout(s_uptime_label);

        lv_coord_t link_w = lv_obj_get_width(s_link_label);
        lv_coord_t uptime_w = lv_obj_get_width(s_uptime_label);

        lv_coord_t link_lbl_x = 778 - link_w;
        lv_coord_t link_ico_x = link_lbl_x - 7 - 18;
        lv_coord_t sep_x = link_ico_x - 14 - 1;
        lv_coord_t uptime_lbl_x = sep_x - 14 - uptime_w;
        lv_coord_t clock_ico_x = uptime_lbl_x - 8 - 20;

        set_pos_if_changed(s_link_label, link_lbl_x, 15);
        set_pos_if_changed(s_link_bars_cont, link_ico_x, 21);
        set_pos_if_changed(s_header_sep, sep_x, 19);
        set_pos_if_changed(s_uptime_label, uptime_lbl_x, 15);
        set_pos_if_changed(s_header_clock_ico, clock_ico_x, 18);
    }
}

/* =========================================================================
 * Bottom Navigation Bar (P1.13 Style Leak Prevention)
 * ========================================================================= */
static void update_navigation_selection(void)
{
    for (int i = 0; i < 3; i++) {
        if (s_nav_btns[i] == NULL) continue;
        if (i == (int)s_current_page) {
            lv_obj_remove_style(s_nav_btns[i], &ui_style_action, 0);
            lv_obj_add_style(s_nav_btns[i], &ui_style_action, 0);
            lv_obj_set_style_img_recolor(s_nav_icons[i], lv_color_white(), 0);
        } else {
            lv_obj_remove_style(s_nav_btns[i], &ui_style_action, 0);
            lv_obj_set_style_img_recolor(s_nav_icons[i], UI_COLOR_MUTED, 0);
        }
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
    lv_obj_set_size(s_nav_bar, 800, 64);
    lv_obj_set_pos(s_nav_bar, 0, 416);
    lv_obj_clear_flag(s_nav_bar, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_set_flex_flow(s_nav_bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(s_nav_bar, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_hor(s_nav_bar, 20, 0);
    lv_obj_set_style_pad_ver(s_nav_bar, 8, 0);
    lv_obj_set_style_pad_column(s_nav_bar, 12, 0);

    const lv_img_dsc_t *icons[3] = {
        &ui_icon_home_32,
        &ui_icon_chart_32,
        &ui_icon_devices_32
    };

    for (uintptr_t i = 0; i < 3; i++) {
        lv_obj_t *btn = lv_btn_create(s_nav_bar);
        lv_obj_remove_style_all(btn);
        lv_obj_set_size(btn, 245, 48);
        lv_obj_set_style_radius(btn, 14, 0);
        lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(btn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_add_event_cb(btn, on_nav_btn_click, LV_EVENT_CLICKED, (void *)i);

        lv_obj_t *ico = add_icon(btn, icons[i], UI_COLOR_MUTED);
        s_nav_btns[i] = btn;
        s_nav_icons[i] = ico;
    }
}

/* =========================================================================
 * Toast Container
 * ========================================================================= */
static void build_toast(void)
{
    s_toast_obj = lv_obj_create(s_screen);
    lv_obj_remove_style_all(s_toast_obj);
    lv_obj_set_size(s_toast_obj, LV_SIZE_CONTENT, 38);
    lv_obj_set_style_bg_color(s_toast_obj, lv_color_hex(0x182B4D), 0);
    lv_obj_set_style_bg_opa(s_toast_obj, 230, 0);
    lv_obj_set_style_radius(s_toast_obj, 19, 0);
    lv_obj_set_style_pad_hor(s_toast_obj, 20, 0);
    lv_obj_set_style_pad_ver(s_toast_obj, 8, 0);
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

static void update_co2_ppm_pos(void)
{
    if (s_home_co2_val != NULL && s_home_co2_ppm_lbl != NULL) {
        lv_obj_update_layout(s_home_co2_val);
        lv_coord_t val_w = lv_obj_get_width(s_home_co2_val);
        set_pos_if_changed(s_home_co2_ppm_lbl, val_w + 8, 74);
    }
}

static void apply_quality_badge(lv_obj_t *box, lv_obj_t *dot, lv_obj_t *lbl, ui_quality_category_t q, char *txt_buf, size_t buf_sz)
{
    lv_color_t color;
    lv_color_t bg_color;
    const char *text;

    switch (q) {
        case UI_QUALITY_GOOD:
            bg_color = UI_COLOR_GREEN_SOFT;
            color = UI_COLOR_GREEN;
            text = "Good";
            break;
        case UI_QUALITY_MODERATE:
            bg_color = UI_COLOR_AMBER_SOFT;
            color = UI_COLOR_AMBER;
            text = "Moderate";
            break;
        case UI_QUALITY_POOR:
            bg_color = UI_COLOR_RED_SOFT;
            color = UI_COLOR_RED;
            text = "Poor";
            break;
        default:
            bg_color = UI_COLOR_NEUTRAL_SOFT;
            color = UI_COLOR_MUTED;
            text = "Unknown";
            break;
    }
    set_bg_color_if_changed(box, bg_color);
    set_bg_color_if_changed(dot, color);
    if (lbl != NULL) {
        set_text_color_if_changed(lbl, color);
        set_buffer_and_label_if_changed(lbl, txt_buf, buf_sz, text);
    }
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
    return "Auto";
}

static void build_home_page(void)
{
    lv_obj_t *p = s_page_container;

    /* 1. CO2 Card (250x208 at x=0, y=0) */
    lv_obj_t *card_co2 = lv_obj_create(p);
    lv_obj_remove_style_all(card_co2);
    lv_obj_set_pos(card_co2, 0, 0);
    lv_obj_set_size(card_co2, 250, 208);
    lv_obj_add_style(card_co2, &ui_style_card_co2, 0);
    lv_obj_set_style_pad_all(card_co2, 16, 0);
    lv_obj_clear_flag(card_co2, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(card_co2, on_home_card_click, LV_EVENT_CLICKED, (void *)UI_METRIC_CO2);

    add_static_label(card_co2, "CO₂", &ui_font_24, UI_COLOR_CO2_FG);
    lv_obj_t *ico_co2 = add_icon(card_co2, &ui_icon_air_28, UI_COLOR_CO2_FG);
    lv_obj_set_pos(ico_co2, 190, 4);

    s_home_co2_val = add_static_label(card_co2, s_co2_val_buf, &ui_font_digits_62, UI_COLOR_CO2_FG);
    lv_obj_set_pos(s_home_co2_val, 0, 42);

    s_home_co2_ppm_lbl = add_static_label(card_co2, "ppm", &ui_font_18, UI_COLOR_CO2_FG);
    update_co2_ppm_pos();

    s_home_co2_badge = lv_obj_create(card_co2);
    lv_obj_remove_style_all(s_home_co2_badge);
    lv_obj_set_pos(s_home_co2_badge, 0, 124);
    lv_obj_set_size(s_home_co2_badge, LV_SIZE_CONTENT, 26);
    lv_obj_set_style_radius(s_home_co2_badge, 8, 0);
    lv_obj_set_style_bg_opa(s_home_co2_badge, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_hor(s_home_co2_badge, 8, 0);
    lv_obj_set_style_pad_ver(s_home_co2_badge, 0, 0);
    lv_obj_set_flex_flow(s_home_co2_badge, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(s_home_co2_badge, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(s_home_co2_badge, 6, 0);

    s_home_co2_badge_dot = lv_obj_create(s_home_co2_badge);
    lv_obj_remove_style_all(s_home_co2_badge_dot);
    lv_obj_set_size(s_home_co2_badge_dot, 6, 6);
    lv_obj_set_style_radius(s_home_co2_badge_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(s_home_co2_badge_dot, LV_OPA_COVER, 0);

    s_home_co2_badge_lbl = add_static_label(s_home_co2_badge, s_co2_badge_buf, &ui_font_18, UI_COLOR_MUTED);

    s_home_co2_note = add_static_label(card_co2, s_co2_note_buf, &ui_font_18, UI_COLOR_INK);
    lv_obj_set_pos(s_home_co2_note, 0, 156);

    /* 2. VOC Card (250x208 at x=262, y=0) */
    lv_obj_t *card_voc = lv_obj_create(p);
    lv_obj_remove_style_all(card_voc);
    lv_obj_set_pos(card_voc, 262, 0);
    lv_obj_set_size(card_voc, 250, 208);
    lv_obj_add_style(card_voc, &ui_style_card_voc, 0);
    lv_obj_set_style_pad_all(card_voc, 16, 0);
    lv_obj_clear_flag(card_voc, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(card_voc, on_home_card_click, LV_EVENT_CLICKED, (void *)UI_METRIC_VOC);

    add_static_label(card_voc, "VOC", &ui_font_24, UI_COLOR_VOC_FG);
    lv_obj_t *ico_voc = add_icon(card_voc, &ui_icon_air_28, UI_COLOR_VOC_FG);
    lv_obj_set_pos(ico_voc, 190, 4);

    s_home_voc_val = add_static_label(card_voc, s_voc_val_buf, &ui_font_digits_62, UI_COLOR_VOC_FG);
    lv_obj_set_pos(s_home_voc_val, 0, 42);

    s_home_voc_badge = lv_obj_create(card_voc);
    lv_obj_remove_style_all(s_home_voc_badge);
    lv_obj_set_pos(s_home_voc_badge, 0, 124);
    lv_obj_set_size(s_home_voc_badge, LV_SIZE_CONTENT, 26);
    lv_obj_set_style_radius(s_home_voc_badge, 8, 0);
    lv_obj_set_style_bg_opa(s_home_voc_badge, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_hor(s_home_voc_badge, 8, 0);
    lv_obj_set_style_pad_ver(s_home_voc_badge, 0, 0);
    lv_obj_set_flex_flow(s_home_voc_badge, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(s_home_voc_badge, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(s_home_voc_badge, 6, 0);

    s_home_voc_badge_dot = lv_obj_create(s_home_voc_badge);
    lv_obj_remove_style_all(s_home_voc_badge_dot);
    lv_obj_set_size(s_home_voc_badge_dot, 6, 6);
    lv_obj_set_style_radius(s_home_voc_badge_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(s_home_voc_badge_dot, LV_OPA_COVER, 0);

    s_home_voc_badge_lbl = add_static_label(s_home_voc_badge, s_voc_badge_buf, &ui_font_18, UI_COLOR_MUTED);

    s_home_voc_note = add_static_label(card_voc, s_voc_note_buf, &ui_font_18, UI_COLOR_INK);
    lv_obj_set_pos(s_home_voc_note, 0, 156);

    /* 3. Temperature Card (236x98 at x=524, y=0) - L2 Balanced Two-line */
    lv_obj_t *card_temp = lv_obj_create(p);
    lv_obj_remove_style_all(card_temp);
    lv_obj_set_pos(card_temp, 524, 0);
    lv_obj_set_size(card_temp, 236, 98);
    lv_obj_add_style(card_temp, &ui_style_card_temp, 0);
    lv_obj_set_style_pad_all(card_temp, 0, 0);
    lv_obj_clear_flag(card_temp, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(card_temp, on_home_card_click, LV_EVENT_CLICKED, (void *)UI_METRIC_TEMP);

    lv_obj_t *ico_temp = add_icon(card_temp, &ui_icon_thermometer_24, UI_COLOR_TEMP_ICON);
    lv_obj_set_pos(ico_temp, 12, 37);

    lv_obj_t *lbl_temp_title = add_static_label(card_temp, "Temperature", &ui_font_18, UI_COLOR_INK);
    lv_obj_set_pos(lbl_temp_title, 44, 12);

    s_home_temp_val = add_static_label(card_temp, s_temp_val_buf, &ui_font_34, UI_COLOR_INK);
    lv_obj_set_pos(s_home_temp_val, 44, 41);

    s_home_temp_note = add_static_label(card_temp, s_temp_note_buf, &ui_font_18, UI_COLOR_MUTED);
    lv_obj_set_pos(s_home_temp_note, 92, 49);

    /* 4. Humidity Card (236x98 at x=524, y=110) - L2 Balanced Two-line */
    lv_obj_t *card_humid = lv_obj_create(p);
    lv_obj_remove_style_all(card_humid);
    lv_obj_set_pos(card_humid, 524, 110);
    lv_obj_set_size(card_humid, 236, 98);
    lv_obj_add_style(card_humid, &ui_style_card_humid, 0);
    lv_obj_set_style_pad_all(card_humid, 0, 0);
    lv_obj_clear_flag(card_humid, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(card_humid, on_home_card_click, LV_EVENT_CLICKED, (void *)UI_METRIC_HUMIDITY);

    lv_obj_t *ico_humid = add_icon(card_humid, &ui_icon_drop_24, UI_COLOR_HUMID_ICON);
    lv_obj_set_pos(ico_humid, 12, 37);

    lv_obj_t *lbl_humid_title = add_static_label(card_humid, "Humidity", &ui_font_18, UI_COLOR_INK);
    lv_obj_set_pos(lbl_humid_title, 44, 12);

    s_home_humid_val = add_static_label(card_humid, s_humid_val_buf, &ui_font_34, UI_COLOR_INK);
    lv_obj_set_pos(s_home_humid_val, 44, 41);

    s_home_humid_note = add_static_label(card_humid, s_humid_note_buf, &ui_font_18, UI_COLOR_MUTED);
    lv_obj_set_pos(s_home_humid_note, 92, 49);

    /* 5. Four Device Tiles (181x116 at y=220) - L3 Collision-free */
    for (uint8_t i = 0; i < UI_DEVICE_COUNT; i++) {
        lv_coord_t x = i * (181 + 12);
        lv_obj_t *tile = lv_obj_create(p);
        lv_obj_remove_style_all(tile);
        lv_obj_set_pos(tile, x, 220);
        lv_obj_set_size(tile, 181, 116);
        lv_obj_add_style(tile, &ui_style_card_device, 0);
        lv_obj_set_style_pad_all(tile, 12, 0);
        lv_obj_clear_flag(tile, LV_OBJ_FLAG_SCROLLABLE);

        s_home_dev_icons[i] = add_icon(tile, get_preset_icon(s_dev_configs[i].preset), UI_COLOR_ICON_BLUE);
        lv_obj_set_pos(s_home_dev_icons[i], 0, 4);

        s_home_dev_names[i] = add_static_label(tile, s_dev_name_buf[i], &ui_font_18, UI_COLOR_INK);
        lv_obj_set_pos(s_home_dev_names[i], 30, 2);
        lv_obj_set_width(s_home_dev_names[i], 130);
        lv_label_set_long_mode(s_home_dev_names[i], LV_LABEL_LONG_WRAP);

        s_home_dev_states[i] = add_static_label(tile, s_dev_state_buf[i], &ui_font_18, UI_COLOR_RED);
        lv_obj_set_pos(s_home_dev_states[i], 0, 63);

        /* Mode label: defaults to left column width 92 with wrap for "Auto\npaused", expands to 157 in composite error */
        s_home_dev_modes[i] = add_static_label(tile, s_dev_mode_buf[i], &ui_font_18, UI_COLOR_MUTED);
        lv_obj_set_pos(s_home_dev_modes[i], 0, 50);
        lv_obj_set_width(s_home_dev_modes[i], 92);
        lv_label_set_long_mode(s_home_dev_modes[i], LV_LABEL_LONG_WRAP);

        s_home_dev_dashes[i] = add_static_label(tile, "—", &ui_font_24, UI_COLOR_MUTED);
        lv_obj_set_pos(s_home_dev_dashes[i], 119, 60);

        s_home_dev_switches[i] = lv_switch_create(tile);
        lv_obj_set_size(s_home_dev_switches[i], 54, 30);
        lv_obj_set_pos(s_home_dev_switches[i], 103, 48);
        lv_obj_set_style_anim_time(s_home_dev_switches[i], 0, 0);
        lv_obj_set_style_bg_color(s_home_dev_switches[i], UI_COLOR_SWITCH_OFF, LV_PART_MAIN);
        lv_obj_set_style_bg_color(s_home_dev_switches[i], UI_COLOR_BLUE, LV_PART_INDICATOR | LV_STATE_CHECKED);
        lv_obj_add_event_cb(s_home_dev_switches[i], on_device_switch_click, LV_EVENT_VALUE_CHANGED, (void *)(uintptr_t)i);

        /* V4: Retry button positioned on Row 2 at (103, 56), size (54, 32) with ext_click_area 6 -> 66x44 touch target inside tile */
        s_home_dev_retries[i] = add_static_label(tile, "Retry", &ui_font_18, UI_COLOR_BLUE);
        lv_obj_set_pos(s_home_dev_retries[i], 103, 56);
        lv_obj_set_size(s_home_dev_retries[i], 54, 32);
        lv_obj_set_ext_click_area(s_home_dev_retries[i], 6);
        lv_obj_add_flag(s_home_dev_retries[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(s_home_dev_retries[i], on_device_retry_click, LV_EVENT_CLICKED, (void *)(uintptr_t)i);
    }

    update_home_page_widgets();
}

static void update_home_page_widgets(void)
{
    if (s_current_page != UI_PAGE_HOME || s_home_co2_val == NULL) return;

    uint32_t now = lv_tick_get();
    bool offline = !is_link_connected(now);

    /* Update CO2 */
    char co2_val_tmp[16];
    char co2_note_tmp[32];
    if (!offline && lora_node_status.bits.co2_valid) {
        uint16_t ppm = lora_node_status.bits.co2_ppm;
        snprintf(co2_val_tmp, sizeof(co2_val_tmp), "%u", (unsigned)ppm);
        ui_quality_category_t q = (ppm <= 800) ? UI_QUALITY_GOOD : ((ppm <= 1200) ? UI_QUALITY_MODERATE : UI_QUALITY_POOR);
        const char *note_str = (q == UI_QUALITY_GOOD) ? "Fresh air" : ((q == UI_QUALITY_MODERATE) ? "Ventilation recommended" : "High CO₂ level");
        snprintf(co2_note_tmp, sizeof(co2_note_tmp), "%s", note_str);
        apply_quality_badge(s_home_co2_badge, s_home_co2_badge_dot, s_home_co2_badge_lbl, q, s_co2_badge_buf, sizeof(s_co2_badge_buf));
    } else {
        snprintf(co2_val_tmp, sizeof(co2_val_tmp), "—");
        snprintf(co2_note_tmp, sizeof(co2_note_tmp), offline ? "Connection lost" : "Sensor unavailable");
        apply_quality_badge(s_home_co2_badge, s_home_co2_badge_dot, s_home_co2_badge_lbl, UI_QUALITY_UNKNOWN, s_co2_badge_buf, sizeof(s_co2_badge_buf));
    }
    if (set_buffer_and_label_if_changed(s_home_co2_val, s_co2_val_buf, sizeof(s_co2_val_buf), co2_val_tmp)) {
        update_co2_ppm_pos();
    }
    set_buffer_and_label_if_changed(s_home_co2_note, s_co2_note_buf, sizeof(s_co2_note_buf), co2_note_tmp);

    /* Update VOC */
    char voc_val_tmp[16];
    char voc_note_tmp[32];
    if (!offline && lora_node_status.bits.voc_valid) {
        uint16_t voc = lora_node_status.bits.voc_index;
        snprintf(voc_val_tmp, sizeof(voc_val_tmp), "%u", (unsigned)voc);
        ui_quality_category_t q = (voc <= 100) ? UI_QUALITY_GOOD : ((voc <= 250) ? UI_QUALITY_MODERATE : UI_QUALITY_POOR);
        const char *note_str = (q == UI_QUALITY_GOOD) ? "Clean air" : ((q == UI_QUALITY_MODERATE) ? "Air quality warning" : "High VOC level");
        snprintf(voc_note_tmp, sizeof(voc_note_tmp), "%s", note_str);
        apply_quality_badge(s_home_voc_badge, s_home_voc_badge_dot, s_home_voc_badge_lbl, q, s_voc_badge_buf, sizeof(s_voc_badge_buf));
    } else {
        snprintf(voc_val_tmp, sizeof(voc_val_tmp), "—");
        snprintf(voc_note_tmp, sizeof(voc_note_tmp), offline ? "Connection lost" : "Sensor unavailable");
        apply_quality_badge(s_home_voc_badge, s_home_voc_badge_dot, s_home_voc_badge_lbl, UI_QUALITY_UNKNOWN, s_voc_badge_buf, sizeof(s_voc_badge_buf));
    }
    set_buffer_and_label_if_changed(s_home_voc_val, s_voc_val_buf, sizeof(s_voc_val_buf), voc_val_tmp);
    set_buffer_and_label_if_changed(s_home_voc_note, s_voc_note_buf, sizeof(s_voc_note_buf), voc_note_tmp);

    /* Update Temperature (L2 Balanced Two-line) */
    char temp_val_tmp[24];
    char temp_note_tmp[32];
    bool temp_valid = !offline && lora_node_status.bits.temp_valid;
    format_metric_value(temp_val_tmp, sizeof(temp_val_tmp), UI_METRIC_TEMP, lora_node_status.bits.temp_deci_c, temp_valid);
    if (temp_valid) {
        temp_note_tmp[0] = '\0';
        set_flag_if_changed(s_home_temp_note, LV_OBJ_FLAG_HIDDEN, true);
    } else {
        snprintf(temp_note_tmp, sizeof(temp_note_tmp), offline ? "Offline" : "No data");
        set_flag_if_changed(s_home_temp_note, LV_OBJ_FLAG_HIDDEN, false);
    }
    set_buffer_and_label_if_changed(s_home_temp_val, s_temp_val_buf, sizeof(s_temp_val_buf), temp_val_tmp);
    set_buffer_and_label_if_changed(s_home_temp_note, s_temp_note_buf, sizeof(s_temp_note_buf), temp_note_tmp);

    /* Update Humidity (L2 Balanced Two-line) */
    char humid_val_tmp[24];
    char humid_note_tmp[32];
    bool humid_valid = !offline && lora_node_status.bits.humid_valid;
    format_metric_value(humid_val_tmp, sizeof(humid_val_tmp), UI_METRIC_HUMIDITY, lora_node_status.bits.humidity_pct, humid_valid);
    if (humid_valid) {
        humid_note_tmp[0] = '\0';
        set_flag_if_changed(s_home_humid_note, LV_OBJ_FLAG_HIDDEN, true);
    } else {
        snprintf(humid_note_tmp, sizeof(humid_note_tmp), offline ? "Offline" : "No data");
        set_flag_if_changed(s_home_humid_note, LV_OBJ_FLAG_HIDDEN, false);
    }
    set_buffer_and_label_if_changed(s_home_humid_val, s_humid_val_buf, sizeof(s_humid_val_buf), humid_val_tmp);
    set_buffer_and_label_if_changed(s_home_humid_note, s_humid_note_buf, sizeof(s_humid_note_buf), humid_note_tmp);

    /* Update 4 Device Tiles (L3 / L4 Link pause & collision-free layout) */
    for (uint8_t i = 0; i < UI_DEVICE_COUNT; i++) {
        const ui_hub_device_config_t *cfg = &s_dev_configs[i];
        ui_device_cmd_state_t *cmd = &s_device_cmds[i];

        if (s_home_dev_icons[i] != NULL) {
            set_img_src_if_changed(s_home_dev_icons[i], get_preset_icon(cfg->preset));
        }

        set_buffer_and_label_if_changed(s_home_dev_names[i], s_dev_name_buf[i], sizeof(s_dev_name_buf[i]), cfg->name);

        const char *mode_str = get_device_mode_display_string(i, offline);
        set_buffer_and_label_if_changed(s_home_dev_modes[i], s_dev_mode_buf[i], sizeof(s_dev_mode_buf[i]), mode_str);

        uint8_t reported_gpio = lora_node_status_get_relay_gpio(&lora_node_status, i);
        bool reported_on = (reported_gpio ^ cfg->active_low) != 0;

        bool is_error = (cmd->phase == UI_CMD_PHASE_ERROR);
        bool is_pending = (cmd->phase == UI_CMD_PHASE_PENDING);
        bool is_latched = (!offline && !is_error && !is_pending && cfg->mode == UI_MODE_AUTO && s_auto_states[i].paused_error);

        const char *new_state_str = "";
        lv_color_t new_state_color = UI_COLOR_RED;
        bool state_hidden = false;
        bool dash_hidden = true;
        bool switch_hidden = true;
        bool switch_disabled = false;
        bool switch_checked = reported_on;
        bool retry_hidden = true;
        bool is_paused = (strcmp(mode_str, "Auto paused") == 0);
        lv_coord_t state_y = 63;
        lv_coord_t mode_y = 50;
        lv_coord_t mode_w = 92;

        if (offline) {
            /* V4 Composite Offline: Row 1 full-width mode (34), Row 2 Unknown (63) + Dash (60) */
            new_state_str = "Unknown";
            new_state_color = UI_COLOR_RED;
            dash_hidden = false;
            state_y = 63;
            mode_y = 34;
            mode_w = 157;
            set_pos_if_changed(s_home_dev_dashes[i], 119, 60);
        } else if (is_error) {
            /* V4 Composite Error: Row 1 full-width mode (34), Row 2 Unknown (63) + Retry (56, touch 66x44) */
            new_state_str = "Unknown";
            new_state_color = UI_COLOR_RED;
            retry_hidden = false;
            state_y = 63;
            mode_y = 34;
            mode_w = 157;
            set_pos_if_changed(s_home_dev_retries[i], 103, 56);
        } else if (is_pending) {
            /* V4 Composite Pending: Row 1 full-width mode (34), Row 2 Sending… (63) + Switch disabled (56) */
            new_state_str = "Sending…";
            new_state_color = UI_COLOR_AMBER;
            switch_hidden = false;
            switch_disabled = true;
            switch_checked = reported_on;
            state_y = 63;
            mode_y = 34;
            mode_w = 157;
            set_pos_if_changed(s_home_dev_switches[i], 103, 56);
        } else if (is_latched) {
            /* W1 Targeted Settled-but-Latched State:
             * Row 1 full-width Auto paused (34, w=157),
             * Row 2 actual readback switch at left (0, 56) and Retry at right (103, 56) */
            state_hidden = true;
            switch_hidden = false;
            switch_disabled = false;
            switch_checked = reported_on;
            retry_hidden = false;
            mode_y = 34;
            mode_w = 157;
            set_pos_if_changed(s_home_dev_switches[i], 0, 56);
            set_pos_if_changed(s_home_dev_retries[i], 103, 56);
        } else {
            /* Settled idle: bounded left column (width 92), switch active at (103, 48) */
            state_hidden = true;
            mode_w = 92;
            mode_y = is_paused ? 42 : 50;
            switch_hidden = false;
            switch_disabled = false;
            switch_checked = reported_on;
            set_pos_if_changed(s_home_dev_switches[i], 103, 48);
        }

        set_buffer_and_label_if_changed(s_home_dev_states[i], s_dev_state_buf[i], sizeof(s_dev_state_buf[i]), new_state_str);
        set_text_color_if_changed(s_home_dev_states[i], new_state_color);
        set_flag_if_changed(s_home_dev_states[i], LV_OBJ_FLAG_HIDDEN, state_hidden);
        set_pos_if_changed(s_home_dev_states[i], 0, state_y);
        set_pos_if_changed(s_home_dev_modes[i], 0, mode_y);
        lv_obj_set_width(s_home_dev_modes[i], mode_w);

        set_flag_if_changed(s_home_dev_dashes[i], LV_OBJ_FLAG_HIDDEN, dash_hidden);
        set_flag_if_changed(s_home_dev_retries[i], LV_OBJ_FLAG_HIDDEN, retry_hidden);

        set_flag_if_changed(s_home_dev_switches[i], LV_OBJ_FLAG_HIDDEN, switch_hidden);
        set_state_if_changed(s_home_dev_switches[i], LV_STATE_DISABLED, switch_disabled);
        set_state_if_changed(s_home_dev_switches[i], LV_STATE_CHECKED, switch_checked);
    }
}

/* =========================================================================
 * TRENDS PAGE (P1.6 Chart point inspection, P1.8 Clock wrap)
 * ========================================================================= */
static void update_trends_tabs(void)
{
    for (uintptr_t i = 0; i < 4; i++) {
        if (s_trends_tab_btns[i] == NULL) continue;
        if (i == (uintptr_t)s_selected_metric) {
            lv_obj_remove_style(s_trends_tab_btns[i], &ui_style_tab_inactive, 0);
            lv_obj_add_style(s_trends_tab_btns[i], &ui_style_tab_active, 0);
            lv_obj_t *lbl = lv_obj_get_child(s_trends_tab_btns[i], 0);
            if (lbl) lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
        } else {
            lv_obj_remove_style(s_trends_tab_btns[i], &ui_style_tab_active, 0);
            lv_obj_add_style(s_trends_tab_btns[i], &ui_style_tab_inactive, 0);
            lv_obj_t *lbl = lv_obj_get_child(s_trends_tab_btns[i], 0);
            if (lbl) lv_obj_set_style_text_color(lbl, UI_COLOR_INK, 0);
        }
    }
}

static void on_metric_tab_click(lv_event_t *e)
{
    uintptr_t metric = (uintptr_t)lv_event_get_user_data(e);
    ui_select_trend_metric((ui_metric_id_t)metric);
}

static void on_chart_pressed(lv_event_t *e)
{
    lv_obj_t *chart = lv_event_get_target(e);
    uint32_t point_id = lv_chart_get_pressed_point(chart);
    if (point_id < UI_HISTORY_CAPACITY && s_trends_tooltip != NULL) {
        s_trends_inspected_point = (int16_t)point_id;
        if (s_history[s_selected_metric][point_id].valid) {
            int32_t val = s_history[s_selected_metric][point_id].value;
            uint16_t m = s_history[s_selected_metric][point_id].minute_of_day % 1440u;
            char val_str[24];
            format_metric_value(val_str, sizeof(val_str), s_selected_metric, val, true);
            snprintf(s_chart_tooltip_buf, sizeof(s_chart_tooltip_buf), "%02u:%02u · %s",
                     m / 60u, m % 60u, val_str);
            update_label_if_changed(s_trends_tooltip, s_chart_tooltip_buf);
            set_flag_if_changed(s_trends_tooltip, LV_OBJ_FLAG_HIDDEN, false);
            lv_obj_align(s_trends_tooltip, LV_ALIGN_TOP_RIGHT, 0, 0);
        } else {
            set_flag_if_changed(s_trends_tooltip, LV_OBJ_FLAG_HIDDEN, true);
            s_trends_inspected_point = -1;
        }
    }
}

static void build_trends_page(void)
{
    lv_obj_t *p = s_page_container;

    /* Title: "Trends" */
    lv_obj_t *lbl_trends_title = add_static_label(p, "Trends", &ui_font_34, UI_COLOR_INK);
    lv_obj_set_pos(lbl_trends_title, 0, -6);

    /* 4 Metric Tabs (height 48 px at y=48, width 184, gap 8) */
    const char *metric_names[4] = { "CO₂", "VOC", "Temperature", "Humidity" };
    for (uintptr_t i = 0; i < 4; i++) {
        lv_coord_t x = i * (184 + 8);
        lv_obj_t *btn = lv_btn_create(p);
        lv_obj_remove_style_all(btn);
        lv_obj_set_pos(btn, x, 48);
        lv_obj_set_size(btn, 184, 48);
        if (i == (uintptr_t)s_selected_metric) {
            lv_obj_add_style(btn, &ui_style_tab_active, 0);
        } else {
            lv_obj_add_style(btn, &ui_style_tab_inactive, 0);
        }
        lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(btn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_add_event_cb(btn, on_metric_tab_click, LV_EVENT_CLICKED, (void *)i);

        lv_color_t txt_col = (i == (uintptr_t)s_selected_metric) ? lv_color_white() : UI_COLOR_INK;
        add_static_label(btn, metric_names[i], &ui_font_18, txt_col);
        s_trends_tab_btns[i] = btn;
    }

    /* Chart Card (760x228 px at y=108) */
    lv_obj_t *card = lv_obj_create(p);
    lv_obj_remove_style_all(card);
    lv_obj_set_pos(card, 0, 108);
    lv_obj_set_size(card, 760, 228);
    lv_obj_add_style(card, &ui_style_card_chart, 0);
    lv_obj_set_style_pad_all(card, 16, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    /* Stats Labels: spread across card at x=0, 240, 480 (screen x=16, 256, 496 relative to card) */
    lv_obj_t *lbl_cur_title = add_static_label(card, "Current", &ui_font_18, UI_COLOR_MUTED);
    lv_obj_set_pos(lbl_cur_title, 0, 0);
    s_trends_cur_lbl = add_static_label(card, s_stat_cur_buf, &ui_font_34, UI_COLOR_BLUE_TEXT);
    lv_obj_set_pos(s_trends_cur_lbl, 0, 22);

    lv_obj_t *lbl_min_title = add_static_label(card, "Min", &ui_font_18, UI_COLOR_MUTED);
    lv_obj_set_pos(lbl_min_title, 240, 0);
    s_trends_min_lbl = add_static_label(card, s_stat_min_buf, &ui_font_34, UI_COLOR_INK);
    lv_obj_set_pos(s_trends_min_lbl, 240, 22);

    lv_obj_t *lbl_max_title = add_static_label(card, "Max", &ui_font_18, UI_COLOR_MUTED);
    lv_obj_set_pos(lbl_max_title, 480, 0);
    s_trends_max_lbl = add_static_label(card, s_stat_max_buf, &ui_font_34, UI_COLOR_INK);
    lv_obj_set_pos(s_trends_max_lbl, 480, 22);

    /* Data unavailable badge */
    s_trends_unavail_badge = lv_obj_create(card);
    lv_obj_remove_style_all(s_trends_unavail_badge);
    lv_obj_set_pos(s_trends_unavail_badge, 580, 0);
    lv_obj_set_size(s_trends_unavail_badge, LV_SIZE_CONTENT, 26);
    lv_obj_set_style_bg_color(s_trends_unavail_badge, UI_COLOR_RED_SOFT, 0);
    lv_obj_set_style_bg_opa(s_trends_unavail_badge, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(s_trends_unavail_badge, 6, 0);
    lv_obj_set_style_pad_hor(s_trends_unavail_badge, 8, 0);
    lv_obj_set_style_pad_ver(s_trends_unavail_badge, 3, 0);
    add_static_label(s_trends_unavail_badge, "Data unavailable", &ui_font_18, UI_COLOR_RED);

    /* Tooltip directly as styled label */
    s_trends_tooltip = lv_label_create(card);
    lv_label_set_text_static(s_trends_tooltip, s_chart_tooltip_buf);
    lv_obj_set_style_text_font(s_trends_tooltip, &ui_font_18, 0);
    lv_obj_set_style_text_color(s_trends_tooltip, lv_color_white(), 0);
    lv_obj_set_style_bg_color(s_trends_tooltip, UI_COLOR_INK, 0);
    lv_obj_set_style_bg_opa(s_trends_tooltip, 230, 0);
    lv_obj_set_style_radius(s_trends_tooltip, 6, 0);
    lv_obj_set_style_pad_hor(s_trends_tooltip, 8, 0);
    lv_obj_set_style_pad_ver(s_trends_tooltip, 3, 0);
    lv_obj_add_flag(s_trends_tooltip, LV_OBJ_FLAG_HIDDEN);

    /* Empty state */
    s_trends_empty_box = lv_obj_create(card);
    lv_obj_remove_style_all(s_trends_empty_box);
    lv_obj_set_pos(s_trends_empty_box, 0, 56);
    lv_obj_set_size(s_trends_empty_box, 728, 130);
    lv_obj_set_flex_flow(s_trends_empty_box, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_trends_empty_box, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    add_icon(s_trends_empty_box, &ui_icon_chart_24, UI_COLOR_MUTED);
    add_static_label(s_trends_empty_box, "No readings yet", &ui_font_24, UI_COLOR_INK);
    add_static_label(s_trends_empty_box, "Waiting for sensor telemetry.", &ui_font_18, UI_COLOR_MUTED);

    /* 3 Y-Axis Labels on the left aligned with 3 horizontal gridlines (y=74, 122, 170) */
    s_trends_y_lbls[0] = add_static_label(card, s_trends_y_buf[0], &ui_font_18, UI_COLOR_MUTED);
    lv_obj_set_pos(s_trends_y_lbls[0], 0, 65);
    s_trends_y_lbls[1] = add_static_label(card, s_trends_y_buf[1], &ui_font_18, UI_COLOR_MUTED);
    lv_obj_set_pos(s_trends_y_lbls[1], 0, 113);
    s_trends_y_lbls[2] = add_static_label(card, s_trends_y_buf[2], &ui_font_18, UI_COLOR_MUTED);
    lv_obj_set_pos(s_trends_y_lbls[2], 0, 161);

    /* Chart (x=54, y=74, w=674, h=96, 3 horizontal lines, 0 vertical) (A3) */
    s_trends_chart = lv_chart_create(card);
    lv_obj_set_pos(s_trends_chart, 54, 74);
    lv_obj_set_size(s_trends_chart, 674, 96);
    lv_obj_set_style_bg_opa(s_trends_chart, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_trends_chart, 0, 0);
    lv_obj_set_style_line_color(s_trends_chart, UI_COLOR_LINE, LV_PART_MAIN);
    lv_chart_set_type(s_trends_chart, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(s_trends_chart, UI_HISTORY_CAPACITY);
    lv_chart_set_div_line_count(s_trends_chart, 3, 0);
    s_trends_chart_series = lv_chart_add_series(s_trends_chart, UI_COLOR_BLUE, LV_CHART_AXIS_PRIMARY_Y);
    lv_chart_set_ext_y_array(s_trends_chart, s_trends_chart_series, s_chart_ext_y);
    lv_obj_set_style_size(s_trends_chart, 6, LV_PART_INDICATOR);
    lv_obj_add_flag(s_trends_chart, LV_OBJ_FLAG_CLICKABLE);
    /* P1.7: Register VALUE_CHANGED only. PRESSED causes duplicate handler invocation during class event dispatch */
    lv_obj_add_event_cb(s_trends_chart, on_chart_pressed, LV_EVENT_VALUE_CHANGED, NULL);

    /* Time Axis row */
    for (int i = 0; i < 4; i++) {
        lv_coord_t x = 54 + i * 224;
        if (i == 3) {
            x = 728 - 42;
        } else if (i > 0) {
            x = x - 20;
        }
        s_trends_time_labels[i] = add_static_label(card, s_time_axis_buf[i], &ui_font_18, UI_COLOR_MUTED);
        lv_obj_set_pos(s_trends_time_labels[i], x, 176);
    }

    update_trends_page_widgets();
}

static void update_trends_page_widgets(void)
{
    if (s_current_page != UI_PAGE_TRENDS || s_trends_cur_lbl == NULL) return;

    if (s_selected_metric != s_last_rendered_metric) {
        update_trends_tabs();
    }

    uint32_t now = lv_tick_get();
    bool offline = !is_link_connected(now);

    int32_t live_val = 0;
    bool live_valid = false;
    if (!offline) {
        switch (s_selected_metric) {
            case UI_METRIC_CO2:
                live_valid = (lora_node_status.bits.co2_valid != 0);
                live_val = (int32_t)lora_node_status.bits.co2_ppm;
                break;
            case UI_METRIC_VOC:
                live_valid = (lora_node_status.bits.voc_valid != 0);
                live_val = (int32_t)lora_node_status.bits.voc_index;
                break;
            case UI_METRIC_TEMP:
                live_valid = (lora_node_status.bits.temp_valid != 0);
                live_val = (int32_t)lora_node_status.bits.temp_deci_c;
                break;
            case UI_METRIC_HUMIDITY:
                live_valid = (lora_node_status.bits.humid_valid != 0);
                live_val = (int32_t)lora_node_status.bits.humidity_pct;
                break;
            default:
                break;
        }
    }
    bool data_missing = !live_valid;

    /* Data Unavailable badge on chart card */
    set_flag_if_changed(s_trends_unavail_badge, LV_OBJ_FLAG_HIDDEN, !data_missing);

    /* Format Summary Stats: Current (A4: cached label check) */
    char cur_tmp[32];
    format_metric_value(cur_tmp, sizeof(cur_tmp), s_selected_metric, live_val, live_valid);
    set_buffer_and_label_if_changed(s_trends_cur_lbl, s_stat_cur_buf, sizeof(s_stat_cur_buf), cur_tmp);

    bool hist_changed = (s_history_rendered_rev[s_selected_metric] != s_history_rev[s_selected_metric] ||
                         s_selected_metric != s_last_rendered_metric);

    if (hist_changed) {
        /* Compute Min/Max over valid samples */
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

        char min_tmp[32];
        char max_tmp[32];
        if (valid_cnt > 0) {
            format_metric_value(min_tmp, sizeof(min_tmp), s_selected_metric, min_v, true);
            format_metric_value(max_tmp, sizeof(max_tmp), s_selected_metric, max_v, true);
        } else {
            snprintf(min_tmp, sizeof(min_tmp), "—");
            snprintf(max_tmp, sizeof(max_tmp), "—");
        }
        set_buffer_and_label_if_changed(s_trends_min_lbl, s_stat_min_buf, sizeof(s_stat_min_buf), min_tmp);
        set_buffer_and_label_if_changed(s_trends_max_lbl, s_stat_max_buf, sizeof(s_stat_max_buf), max_tmp);

        /* Plot area or Empty State */
        if (valid_cnt == 0) {
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
            for (int i = 0; i < 4; i++) {
                if (s_trends_time_labels[i]) set_flag_if_changed(s_trends_time_labels[i], LV_OBJ_FLAG_HIDDEN, false);
            }

            int32_t span = max_v - min_v;
            if (span < 10) span = (s_selected_metric == UI_METRIC_TEMP) ? 10 : 20;
            int32_t range_min = min_v - (span * 2) / 10;
            int32_t range_max = max_v + (span * 2) / 10;
            if (range_min != s_last_trends_range_min || range_max != s_last_trends_range_max) {
                s_last_trends_range_min = range_min;
                s_last_trends_range_max = range_max;
                lv_chart_set_range(s_trends_chart, LV_CHART_AXIS_PRIMARY_Y, range_min, range_max);
            }

            /* Update 3 Y-Axis Labels via temporary buffers (A3) */
            int32_t range_mid = (range_min + range_max) / 2;
            char y_tmp[3][16];
            if (s_selected_metric == UI_METRIC_TEMP) {
                format_temp_c(y_tmp[0], sizeof(y_tmp[0]), range_max);
                format_temp_c(y_tmp[1], sizeof(y_tmp[1]), range_mid);
                format_temp_c(y_tmp[2], sizeof(y_tmp[2]), range_min);
            } else {
                snprintf(y_tmp[0], sizeof(y_tmp[0]), "%d", (int)range_max);
                snprintf(y_tmp[1], sizeof(y_tmp[1]), "%d", (int)range_mid);
                snprintf(y_tmp[2], sizeof(y_tmp[2]), "%d", (int)range_min);
            }
            for (int i = 0; i < 3; i++) {
                if (s_trends_y_lbls[i]) {
                    set_buffer_and_label_if_changed(s_trends_y_lbls[i], s_trends_y_buf[i], sizeof(s_trends_y_buf[i]), y_tmp[i]);
                    set_flag_if_changed(s_trends_y_lbls[i], LV_OBJ_FLAG_HIDDEN, false);
                }
            }

            /* Fill points with gaps for missing readings */
            bool points_changed = false;
            for (uint16_t i = 0; i < UI_HISTORY_CAPACITY; i++) {
                lv_coord_t new_y = (i < s_history_count[s_selected_metric] && s_history[s_selected_metric][i].valid) ?
                                   (lv_coord_t)s_history[s_selected_metric][i].value : LV_CHART_POINT_NONE;
                if (s_chart_ext_y[i] != new_y) {
                    s_chart_ext_y[i] = new_y;
                    points_changed = true;
                }
            }
            if (points_changed) {
                lv_chart_refresh(s_trends_chart);
            }
        }

        /* Update time axis labels (P1.8 modulo 1440) via temporary buffer */
        uint16_t start_m = (s_history_count[s_selected_metric] > 0) ?
                           s_history[s_selected_metric][0].minute_of_day : (14u * 60u + 17u);
        for (int i = 0; i < 4; i++) {
            uint16_t m = (start_m + (uint16_t)(i * 5)) % 1440u;
            char time_tmp[8];
            snprintf(time_tmp, sizeof(time_tmp), "%02u:%02u", m / 60u, m % 60u);
            set_buffer_and_label_if_changed(s_trends_time_labels[i], s_time_axis_buf[i], sizeof(s_time_axis_buf[i]), time_tmp);
        }

        /* Refresh or clear inspected tooltip (A4) */
        if (s_trends_inspected_point >= 0 && s_trends_tooltip != NULL) {
            uint16_t pt = (uint16_t)s_trends_inspected_point;
            if (pt < s_history_count[s_selected_metric] && s_history[s_selected_metric][pt].valid) {
                int32_t val = s_history[s_selected_metric][pt].value;
                uint16_t m = s_history[s_selected_metric][pt].minute_of_day % 1440u;
                char val_str[24];
                format_metric_value(val_str, sizeof(val_str), s_selected_metric, val, true);
                char tip_tmp[64];
                snprintf(tip_tmp, sizeof(tip_tmp), "%02u:%02u · %s", m / 60u, m % 60u, val_str);
                set_buffer_and_label_if_changed(s_trends_tooltip, s_chart_tooltip_buf, sizeof(s_chart_tooltip_buf), tip_tmp);
                set_flag_if_changed(s_trends_tooltip, LV_OBJ_FLAG_HIDDEN, false);
            } else {
                set_flag_if_changed(s_trends_tooltip, LV_OBJ_FLAG_HIDDEN, true);
                s_trends_inspected_point = -1;
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

    /* Title: "Devices" */
    lv_obj_t *lbl_dev_title = add_static_label(p, "Devices", &ui_font_34, UI_COLOR_INK);
    lv_obj_set_pos(lbl_dev_title, 0, -6);

    /* 2x2 Grid of cards (374x136 each, gap 12) */
    for (uint8_t i = 0; i < UI_DEVICE_COUNT; i++) {
        lv_coord_t col = (i % 2);
        lv_coord_t row = (i / 2);
        lv_coord_t x = col * (374 + 12);
        lv_coord_t y = 48 + row * (136 + 12);

        lv_obj_t *card = lv_obj_create(p);
        lv_obj_remove_style_all(card);
        lv_obj_set_pos(card, x, y);
        lv_obj_set_size(card, 374, 136);
        lv_obj_add_style(card, &ui_style_card_device, 0);
        lv_obj_set_style_pad_hor(card, 16, 0);
        lv_obj_set_style_pad_ver(card, 14, 0);
        lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

        /* Icon Plate (46x46 at 0, 0, radius 12, #DEEBFF) */
        lv_obj_t *plate = lv_obj_create(card);
        lv_obj_remove_style_all(plate);
        lv_obj_set_pos(plate, 0, 0);
        lv_obj_set_size(plate, 46, 46);
        lv_obj_set_style_radius(plate, 12, 0);
        lv_obj_set_style_bg_color(plate, UI_COLOR_PLATE_BLUE, 0);
        lv_obj_set_style_bg_opa(plate, LV_OPA_COVER, 0);
        lv_obj_set_flex_flow(plate, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(plate, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        s_dev_page_icons[i] = add_icon(plate, get_preset_icon(s_dev_configs[i].preset), UI_COLOR_ICON_BLUE);

        /* Name Label (font 24, pos 58, 6, width 232 px) */
        s_dev_page_names[i] = add_static_label(card, s_dev_name_buf[i], &ui_font_24, UI_COLOR_INK);
        lv_obj_set_pos(s_dev_page_names[i], 58, 6);
        lv_obj_set_width(s_dev_page_names[i], 232);
        lv_label_set_long_mode(s_dev_page_names[i], LV_LABEL_LONG_CLIP);

        /* Configure Button (Sliders) */
        lv_obj_t *btn_cfg = add_icon(card, &ui_icon_sliders_24, UI_COLOR_MUTED);
        lv_obj_set_pos(btn_cfg, 342 - 36, 5);
        lv_obj_set_size(btn_cfg, 36, 36);
        lv_obj_set_style_pad_all(btn_cfg, 6, 0);
        lv_obj_add_flag(btn_cfg, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(btn_cfg, on_device_configure_click, LV_EVENT_CLICKED, (void *)(uintptr_t)i);

        /* Separator Hairline (1px line at y=54) */
        lv_obj_t *sep = lv_obj_create(card);
        lv_obj_remove_style_all(sep);
        lv_obj_set_pos(sep, 0, 54);
        lv_obj_set_size(sep, 342, 1);
        lv_obj_set_style_bg_color(sep, UI_COLOR_LINE, 0);
        lv_obj_set_style_bg_opa(sep, LV_OPA_COVER, 0);

        /* Bottom Row: Mode Label & Optional State Feedback */
        s_dev_page_modes[i] = add_static_label(card, s_dev_mode_buf[i], &ui_font_24, UI_COLOR_INK);
        lv_obj_set_pos(s_dev_page_modes[i], 0, 69);

        s_dev_page_states[i] = add_static_label(card, s_dev_state_buf[i], &ui_font_24, UI_COLOR_AMBER);
        lv_obj_set_pos(s_dev_page_states[i], 76, 69);

        s_dev_page_dashes[i] = add_static_label(card, "—", &ui_font_24, UI_COLOR_MUTED);
        lv_obj_set_pos(s_dev_page_dashes[i], 304, 68);

        s_dev_page_switches[i] = lv_switch_create(card);
        lv_obj_set_size(s_dev_page_switches[i], 54, 30);
        lv_obj_set_pos(s_dev_page_switches[i], 342 - 54, 72);
        lv_obj_set_style_anim_time(s_dev_page_switches[i], 0, 0);
        lv_obj_set_style_bg_color(s_dev_page_switches[i], UI_COLOR_SWITCH_OFF, LV_PART_MAIN);
        lv_obj_set_style_bg_color(s_dev_page_switches[i], UI_COLOR_BLUE, LV_PART_INDICATOR | LV_STATE_CHECKED);
        lv_obj_add_event_cb(s_dev_page_switches[i], on_device_switch_click, LV_EVENT_VALUE_CHANGED, (void *)(uintptr_t)i);

        s_dev_page_retries[i] = add_static_label(card, "Retry", &ui_font_18, UI_COLOR_BLUE);
        lv_obj_set_pos(s_dev_page_retries[i], 342 - 58, 65);
        lv_obj_set_size(s_dev_page_retries[i], 58, 44);
        lv_obj_set_ext_click_area(s_dev_page_retries[i], 4);
        lv_obj_add_flag(s_dev_page_retries[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(s_dev_page_retries[i], on_device_retry_click, LV_EVENT_CLICKED, (void *)(uintptr_t)i);
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
            set_img_src_if_changed(s_dev_page_icons[i], get_preset_icon(cfg->preset));
        }

        set_buffer_and_label_if_changed(s_dev_page_names[i], s_dev_name_buf[i], sizeof(s_dev_name_buf[i]), cfg->name);

        const char *mode_str = get_device_mode_display_string(i, offline);
        set_buffer_and_label_if_changed(s_dev_page_modes[i], s_dev_mode_buf[i], sizeof(s_dev_mode_buf[i]), mode_str);

        uint8_t reported_gpio = lora_node_status_get_relay_gpio(&lora_node_status, i);
        bool reported_on = (reported_gpio ^ cfg->active_low) != 0;

        bool is_error = (cmd->phase == UI_CMD_PHASE_ERROR);
        bool is_pending = (cmd->phase == UI_CMD_PHASE_PENDING);
        bool is_latched = (!offline && !is_error && !is_pending && cfg->mode == UI_MODE_AUTO && s_auto_states[i].paused_error);

        const char *new_state_str = "";
        bool state_hidden = false;
        bool dash_hidden = true;
        bool switch_hidden = true;
        bool switch_disabled = false;
        bool switch_checked = reported_on;
        bool retry_hidden = true;

        if (offline) {
            new_state_str = "Unknown";
            dash_hidden = false;
        } else if (is_error) {
            new_state_str = "Unknown";
            retry_hidden = false;
            set_pos_if_changed(s_dev_page_retries[i], 342 - 58, 65);
        } else if (is_pending) {
            new_state_str = "Sending…";
            switch_hidden = false;
            switch_disabled = true;
            switch_checked = reported_on;
            set_pos_if_changed(s_dev_page_switches[i], 342 - 54, 72);
        } else if (is_latched) {
            /* W1 Targeted Settled-but-Latched State:
             * Mode at left (0, 69), actual readback switch at (218, 72) and Retry at (284, 65) */
            state_hidden = true;
            switch_hidden = false;
            switch_disabled = false;
            switch_checked = reported_on;
            retry_hidden = false;
            set_pos_if_changed(s_dev_page_switches[i], 218, 72);
            set_pos_if_changed(s_dev_page_retries[i], 342 - 58, 65);
        } else {
            /* Settled idle: mode only, normal state expressed by switch at (288, 72) */
            state_hidden = true;
            switch_hidden = false;
            switch_disabled = false;
            switch_checked = reported_on;
            set_pos_if_changed(s_dev_page_switches[i], 342 - 54, 72);
        }

        set_buffer_and_label_if_changed(s_dev_page_states[i], s_dev_state_buf[i], sizeof(s_dev_state_buf[i]), new_state_str);
        set_text_color_if_changed(s_dev_page_states[i], UI_COLOR_AMBER);
        set_flag_if_changed(s_dev_page_states[i], LV_OBJ_FLAG_HIDDEN, state_hidden);

        /* P1.5: Position mode and state labels directly using font metrics. */
        lv_point_t mode_sz;
        lv_txt_get_size(&mode_sz, mode_str, &ui_font_24, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
        set_pos_if_changed(s_dev_page_modes[i], 0, 69);
        set_pos_if_changed(s_dev_page_states[i], mode_sz.x + 14, 69);

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
        ui_announce("Cannot switch: disconnected from node.");
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
        ui_announce("Auto control re-armed.");
        if (s_current_page == UI_PAGE_HOME) update_home_page_widgets();
        else if (s_current_page == UI_PAGE_DEVICES) update_devices_page_widgets();
        return;
    }

    /* Accepted user switch gesture in Auto atomically commits Manual mode */
    if (!is_retry && cfg->mode == UI_MODE_AUTO) {
        s_dev_configs[idx].mode = UI_MODE_MANUAL;
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

    ui_announce(is_retry ? "Retrying command…" : "Sending to node…");

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
 * DEVICE SETTINGS DIALOG (MODAL WITH AUTO THRESHOLDS SUBVIEW)
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

static void on_dialog_active_low_change(lv_event_t *e)
{
    lv_obj_t *sw = lv_event_get_target(e);
    if (sw != NULL) {
        s_dialog_edit_active_low = lv_obj_has_state(sw, LV_STATE_CHECKED) ? 1u : 0u;
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
        if (list != NULL) {
            lv_obj_add_style(list, &ui_style_dd_list, LV_PART_MAIN);
            lv_obj_add_style(list, &ui_style_dd_list_selected, LV_PART_SELECTED);
            lv_obj_t *lbl = lv_obj_get_child(list, 0);
            if (lbl != NULL) {
                lv_obj_set_style_text_font(lbl, &ui_font_18, LV_PART_MAIN);
                lv_obj_set_style_text_line_space(lbl, 19, LV_PART_MAIN);
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

    for (int i = 0; i < 2; i++) {
        if (s_dialog_mode_btns[i] == NULL) continue;
        bool is_sel = (s_dialog_btn_modes[i] == s_dialog_edit_mode);
        lv_obj_remove_style(s_dialog_mode_btns[i], &ui_style_dialog_btn_on, 0);
        lv_obj_remove_style(s_dialog_mode_btns[i], &ui_style_dialog_btn_off, 0);
        lv_obj_add_style(s_dialog_mode_btns[i], is_sel ? &ui_style_dialog_btn_on : &ui_style_dialog_btn_off, 0);
        if (s_dialog_mode_labels[i]) {
            lv_obj_set_style_text_color(s_dialog_mode_labels[i], is_sel ? UI_COLOR_BLUE_TEXT : UI_COLOR_INK, 0);
        }
    }

    bool supported = ui_auto_is_preset_supported(s_dialog_edit_preset);
    if (supported) {
        if (s_dialog_btn_limits) lv_obj_clear_flag(s_dialog_btn_limits, LV_OBJ_FLAG_HIDDEN);
        if (s_dialog_mode_helper_lbl) lv_obj_add_flag(s_dialog_mode_helper_lbl, LV_OBJ_FLAG_HIDDEN);
    } else {
        if (s_dialog_btn_limits) lv_obj_add_flag(s_dialog_btn_limits, LV_OBJ_FLAG_HIDDEN);
        if (s_dialog_mode_helper_lbl) lv_obj_clear_flag(s_dialog_mode_helper_lbl, LV_OBJ_FLAG_HIDDEN);
    }

    if (s_dialog_btn_save_dev != NULL && s_dialog_device_idx >= 0 && (uint8_t)s_dialog_device_idx < UI_DEVICE_COUNT) {
        ui_hub_device_config_t cand = s_dev_configs[s_dialog_device_idx];
        cand.preset = s_dialog_edit_preset;
        cand.mode = s_dialog_edit_mode;
        cand.active_low = s_dialog_edit_active_low;
        cand.auto_on_thresh = s_dialog_edit_on_thresh;
        cand.auto_off_thresh = s_dialog_edit_off_thresh;
        cand.schema_version = UI_CONFIG_SCHEMA_VERSION;
        snprintf(cand.name, sizeof(cand.name), "%s", s_presets[cand.preset].name);

        char err_buf[64];
        bool valid = ui_auto_validate_config(&cand, err_buf, sizeof(err_buf));
        if (valid) {
            lv_obj_clear_state(s_dialog_btn_save_dev, LV_STATE_DISABLED);
        } else {
            lv_obj_add_state(s_dialog_btn_save_dev, LV_STATE_DISABLED);
        }
    }
}

static void update_dialog_auto_subview(void)
{
    if (s_dialog_subview_auto == NULL) return;
    const ui_auto_rule_info_t *rule = ui_auto_get_rule_info(s_dialog_edit_preset);
    if (rule == NULL) return;

    if (rule->unit_name != NULL && rule->unit_name[0] != '\0') {
        snprintf(s_dialog_auto_rule_buf, sizeof(s_dialog_auto_rule_buf), "%s — %s (%s)",
                 s_presets[s_dialog_edit_preset].name, rule->sensor_name, rule->unit_name);
    } else {
        snprintf(s_dialog_auto_rule_buf, sizeof(s_dialog_auto_rule_buf), "%s — %s",
                 s_presets[s_dialog_edit_preset].name, rule->sensor_name);
    }
    if (s_dialog_auto_rule_lbl != NULL) lv_label_set_text_static(s_dialog_auto_rule_lbl, s_dialog_auto_rule_buf);

    snprintf(s_dialog_on_op_buf, sizeof(s_dialog_on_op_buf), "Turn on at %s", rule->on_op);
    if (s_dialog_on_op_lbl != NULL) lv_label_set_text_static(s_dialog_on_op_lbl, s_dialog_on_op_buf);

    snprintf(s_dialog_off_op_buf, sizeof(s_dialog_off_op_buf), "Turn off at %s", rule->off_op);
    if (s_dialog_off_op_lbl != NULL) lv_label_set_text_static(s_dialog_off_op_lbl, s_dialog_off_op_buf);

    snprintf(s_dialog_on_val_buf, sizeof(s_dialog_on_val_buf), "%d", (int)s_dialog_edit_on_thresh);
    if (s_dialog_on_val_lbl != NULL) lv_label_set_text_static(s_dialog_on_val_lbl, s_dialog_on_val_buf);

    snprintf(s_dialog_off_val_buf, sizeof(s_dialog_off_val_buf), "%d", (int)s_dialog_edit_off_thresh);
    if (s_dialog_off_val_lbl != NULL) lv_label_set_text_static(s_dialog_off_val_lbl, s_dialog_off_val_buf);

    char err_msg[64] = "";
    bool valid = ui_auto_validate_thresholds(s_dialog_edit_preset, s_dialog_edit_on_thresh, s_dialog_edit_off_thresh, err_msg, sizeof(err_msg));
    snprintf(s_dialog_validation_buf, sizeof(s_dialog_validation_buf), "%s", err_msg);
    if (s_dialog_validation_lbl != NULL) {
        lv_label_set_text_static(s_dialog_validation_lbl, s_dialog_validation_buf);
        if (valid) {
            lv_obj_add_flag(s_dialog_validation_lbl, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_clear_flag(s_dialog_validation_lbl, LV_OBJ_FLAG_HIDDEN);
        }
    }

    if (s_dialog_btn_save_auto != NULL) {
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
        ui_auto_restart_hold(&s_auto_states[idx], now);
    } else if (limits_changed && cand->mode == UI_MODE_AUTO) {
        ui_auto_reset_qualification(&s_auto_states[idx]);
        ui_auto_restart_hold(&s_auto_states[idx], now);
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
    snprintf(cand.name, sizeof(cand.name), "%s", s_presets[cand.preset].name);

    char err_msg[64] = {0};
    if (!commit_device_config_internal(idx, &cand, false, err_msg, sizeof(err_msg))) {
        ui_announce(err_msg[0] ? err_msg : "Invalid settings draft.");
        return;
    }

    close_device_dialog();
    ui_announce("Device settings saved.");
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
        s_dialog_validation_lbl = NULL;
    }

    s_dialog_subview_device = lv_obj_create(box);
    lv_obj_remove_style_all(s_dialog_subview_device);
    lv_obj_set_pos(s_dialog_subview_device, 0, 0);
    lv_obj_set_size(s_dialog_subview_device, 400, 276);
    lv_obj_clear_flag(s_dialog_subview_device, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(s_dialog_subview_device, LV_OBJ_FLAG_CLICKABLE);

    /* Common Header (y=0..43): Title vertically centered (y=8), ends before X at 356 */
    lv_obj_t *lbl_dialog_title = add_static_label(s_dialog_subview_device, "Device settings", &ui_font_24, UI_COLOR_INK);
    lv_obj_set_pos(lbl_dialog_title, 0, 8);

    /* Row 1 (y=56, h=44): Device type, Label x=0,w=128; dropdown x=140,w=260 */
    lv_obj_t *lbl_t = add_static_label(s_dialog_subview_device, "Device type", &ui_font_18, UI_COLOR_MUTED);
    lv_obj_set_pos(lbl_t, 0, 66);
    lv_obj_set_width(lbl_t, 128);

    s_dialog_preset_dd = lv_dropdown_create(s_dialog_subview_device);
    lv_obj_set_pos(s_dialog_preset_dd, 140, 56);
    lv_obj_set_size(s_dialog_preset_dd, 260, 44);
    lv_obj_set_style_border_width(s_dialog_preset_dd, 0, 0);
    lv_obj_set_style_radius(s_dialog_preset_dd, 10, 0);
    lv_obj_set_style_bg_color(s_dialog_preset_dd, lv_color_hex(0xF7F7F7), 0);
    lv_obj_set_style_text_font(s_dialog_preset_dd, &ui_font_18, 0);
    lv_obj_set_style_pad_hor(s_dialog_preset_dd, 12, 0);
    lv_obj_set_style_pad_ver(s_dialog_preset_dd, 10, 0);
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

    /* V2: Style and bound the open dropdown list using static styles */
    lv_obj_t *list = lv_dropdown_get_list(s_dialog_preset_dd);
    if (list != NULL) {
        lv_obj_add_style(list, &ui_style_dd_list, LV_PART_MAIN);
        lv_obj_add_style(list, &ui_style_dd_list_selected, LV_PART_SELECTED);
        lv_obj_t *lbl = lv_obj_get_child(list, 0);
        if (lbl != NULL) {
            lv_obj_set_style_text_font(lbl, &ui_font_18, LV_PART_MAIN);
            lv_obj_set_style_text_line_space(lbl, 19, LV_PART_MAIN);
        }
    }

    /* Row 2 (y=112, h=44): Control mode, Label x=0,w=128; Manual x=140,w=124; Auto x=276,w=124 */
    lv_obj_t *lbl_m = add_static_label(s_dialog_subview_device, "Control mode", &ui_font_18, UI_COLOR_MUTED);
    lv_obj_set_pos(lbl_m, 0, 122);
    lv_obj_set_width(lbl_m, 128);

    const char *modes[2] = { "Manual", "Auto" };
    for (uintptr_t i = 0; i < 2; i++) {
        lv_obj_t *b = lv_btn_create(s_dialog_subview_device);
        lv_obj_remove_style_all(b);
        lv_obj_set_pos(b, (i == 0) ? 140 : 276, 112);
        lv_obj_set_size(b, 124, 44);

        bool is_sel = (s_dialog_btn_modes[i] == s_dialog_edit_mode);
        lv_obj_add_style(b, is_sel ? &ui_style_dialog_btn_on : &ui_style_dialog_btn_off, 0);
        lv_obj_add_event_cb(b, on_dialog_mode_btn_click, LV_EVENT_CLICKED, (void *)(uintptr_t)s_dialog_btn_modes[i]);

        s_dialog_mode_labels[i] = add_static_label(b, modes[i], &ui_font_18,
                                                   is_sel ? UI_COLOR_BLUE_TEXT : UI_COLOR_INK);
        lv_obj_center(s_dialog_mode_labels[i]);
        s_dialog_mode_btns[i] = b;
    }

    /* Row 3 (y=168, h=44): Full-width secondary action "Set limits…" x=0,w=400 */
    s_dialog_btn_limits = lv_btn_create(s_dialog_subview_device);
    lv_obj_remove_style_all(s_dialog_btn_limits);
    lv_obj_set_pos(s_dialog_btn_limits, 0, 168);
    lv_obj_set_size(s_dialog_btn_limits, 400, 44);
    lv_obj_add_style(s_dialog_btn_limits, &ui_style_soft_btn, 0);
    lv_obj_add_flag(s_dialog_btn_limits, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_dialog_btn_limits, on_dialog_open_limits_click, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_limits = add_static_label(s_dialog_btn_limits, "Set limits…", &ui_font_18, UI_COLOR_BLUE_TEXT);
    lv_obj_center(lbl_limits);

    /* Helper label for unsupported presets in Row 3 (y=168, h=44, centered y=178) */
    s_dialog_mode_helper_lbl = add_static_label(s_dialog_subview_device, "Manual only for this device type", &ui_font_18, UI_COLOR_MUTED);
    lv_obj_set_pos(s_dialog_mode_helper_lbl, 0, 178);

    /* Row 4 (y=224, h=44): Active low polarity, Label left; visual switch right (>=54x44 target) */
    lv_obj_t *lbl_pol = add_static_label(s_dialog_subview_device, "Active low polarity", &ui_font_18, UI_COLOR_MUTED);
    lv_obj_set_pos(lbl_pol, 0, 234);

    s_dialog_active_low_sw = lv_switch_create(s_dialog_subview_device);
    lv_obj_set_size(s_dialog_active_low_sw, 54, 30);
    lv_obj_set_pos(s_dialog_active_low_sw, 400 - 54, 231);
    lv_obj_set_ext_click_area(s_dialog_active_low_sw, 7);
    lv_obj_set_style_bg_color(s_dialog_active_low_sw, UI_COLOR_SWITCH_OFF, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_dialog_active_low_sw, UI_COLOR_BLUE, LV_PART_INDICATOR | LV_STATE_CHECKED);
    if (s_dialog_edit_active_low) {
        lv_obj_add_state(s_dialog_active_low_sw, LV_STATE_CHECKED);
    }
    lv_obj_add_event_cb(s_dialog_active_low_sw, on_dialog_active_low_change, LV_EVENT_VALUE_CHANGED, NULL);

    update_dialog_device_subview();
    if (s_dialog_btn_x != NULL) lv_obj_move_foreground(s_dialog_btn_x);
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
        s_dialog_mode_btns[0] = NULL;
        s_dialog_mode_btns[1] = NULL;
        s_dialog_mode_labels[0] = NULL;
        s_dialog_mode_labels[1] = NULL;
        s_dialog_active_low_sw = NULL;
        s_dialog_btn_limits = NULL;
        s_dialog_mode_helper_lbl = NULL;
    }

    s_dialog_subview_auto = lv_obj_create(box);
    lv_obj_remove_style_all(s_dialog_subview_auto);
    lv_obj_set_pos(s_dialog_subview_auto, 0, 0);
    lv_obj_set_size(s_dialog_subview_auto, 400, 276);
    lv_obj_clear_flag(s_dialog_subview_auto, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(s_dialog_subview_auto, LV_OBJ_FLAG_CLICKABLE);

    /* Common Header (y=0..43): Back button at (0, 0, 44, 44) */
    lv_obj_t *btn_back = lv_obj_create(s_dialog_subview_auto);
    lv_obj_remove_style_all(btn_back);
    lv_obj_set_pos(btn_back, 0, 0);
    lv_obj_set_size(btn_back, 44, 44);
    lv_obj_add_style(btn_back, &ui_style_soft_btn, 0);
    lv_obj_add_flag(btn_back, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(btn_back, on_dialog_limits_back_click, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_back = add_static_label(btn_back, "<", &ui_font_24, UI_COLOR_INK);
    lv_obj_center(lbl_back);

    /* Heading: "Auto thresholds" starts x=56, ends by x=344, y=8 vertically centered */
    lv_obj_t *lbl_auto_title = add_static_label(s_dialog_subview_auto, "Auto thresholds", &ui_font_24, UI_COLOR_INK);
    lv_obj_set_pos(lbl_auto_title, 56, 8);

    /* Rule context at x=0, y=56, w=400, h=25 */
    s_dialog_auto_rule_lbl = add_static_label(s_dialog_subview_auto, "", &ui_font_18, UI_COLOR_MUTED);
    lv_obj_set_pos(s_dialog_auto_rule_lbl, 0, 56);
    lv_obj_set_size(s_dialog_auto_rule_lbl, 400, 25);

    /* ON row at y=92, h=48: condition x=0,w=136; minus x=148,w=48; val x=208,w=132; plus x=352,w=48 */
    s_dialog_on_op_lbl = add_static_label(s_dialog_subview_auto, "Turn on at >=", &ui_font_18, UI_COLOR_INK);
    lv_obj_set_pos(s_dialog_on_op_lbl, 0, 104);
    lv_obj_set_width(s_dialog_on_op_lbl, 136);

    lv_obj_t *btn_on_minus = lv_obj_create(s_dialog_subview_auto);
    lv_obj_remove_style_all(btn_on_minus);
    lv_obj_set_pos(btn_on_minus, 148, 92);
    lv_obj_set_size(btn_on_minus, 48, 48);
    lv_obj_add_style(btn_on_minus, &ui_style_soft_btn, 0);
    lv_obj_add_flag(btn_on_minus, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(btn_on_minus, on_thresh_step_click, LV_EVENT_SHORT_CLICKED, (void *)(uintptr_t)1);
    lv_obj_add_event_cb(btn_on_minus, on_thresh_step_click, LV_EVENT_LONG_PRESSED_REPEAT, (void *)(uintptr_t)1);
    lv_obj_t *lbl_on_minus = add_static_label(btn_on_minus, "—", &ui_font_18, UI_COLOR_INK);
    lv_obj_center(lbl_on_minus);

    s_dialog_on_val_lbl = lv_label_create(s_dialog_subview_auto);
    lv_label_set_text_static(s_dialog_on_val_lbl, s_dialog_on_val_buf);
    lv_obj_add_style(s_dialog_on_val_lbl, &ui_style_lbl_24_center, 0);
    lv_obj_set_pos(s_dialog_on_val_lbl, 208, 102);
    lv_obj_set_width(s_dialog_on_val_lbl, 132);

    lv_obj_t *btn_on_plus = lv_obj_create(s_dialog_subview_auto);
    lv_obj_remove_style_all(btn_on_plus);
    lv_obj_set_pos(btn_on_plus, 352, 92);
    lv_obj_set_size(btn_on_plus, 48, 48);
    lv_obj_add_style(btn_on_plus, &ui_style_soft_btn, 0);
    lv_obj_add_flag(btn_on_plus, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(btn_on_plus, on_thresh_step_click, LV_EVENT_SHORT_CLICKED, (void *)(uintptr_t)0);
    lv_obj_add_event_cb(btn_on_plus, on_thresh_step_click, LV_EVENT_LONG_PRESSED_REPEAT, (void *)(uintptr_t)0);
    lv_obj_t *lbl_on_plus = add_static_label(btn_on_plus, "+", &ui_font_24, UI_COLOR_INK);
    lv_obj_center(lbl_on_plus);

    /* OFF row at y=152, h=48: condition x=0,w=136; minus x=148,w=48; val x=208,w=132; plus x=352,w=48 */
    s_dialog_off_op_lbl = add_static_label(s_dialog_subview_auto, "Turn off at <=", &ui_font_18, UI_COLOR_INK);
    lv_obj_set_pos(s_dialog_off_op_lbl, 0, 164);
    lv_obj_set_width(s_dialog_off_op_lbl, 136);

    lv_obj_t *btn_off_minus = lv_obj_create(s_dialog_subview_auto);
    lv_obj_remove_style_all(btn_off_minus);
    lv_obj_set_pos(btn_off_minus, 148, 152);
    lv_obj_set_size(btn_off_minus, 48, 48);
    lv_obj_add_style(btn_off_minus, &ui_style_soft_btn, 0);
    lv_obj_add_flag(btn_off_minus, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(btn_off_minus, on_thresh_step_click, LV_EVENT_SHORT_CLICKED, (void *)(uintptr_t)3);
    lv_obj_add_event_cb(btn_off_minus, on_thresh_step_click, LV_EVENT_LONG_PRESSED_REPEAT, (void *)(uintptr_t)3);
    lv_obj_t *lbl_off_minus = add_static_label(btn_off_minus, "—", &ui_font_18, UI_COLOR_INK);
    lv_obj_center(lbl_off_minus);

    s_dialog_off_val_lbl = lv_label_create(s_dialog_subview_auto);
    lv_label_set_text_static(s_dialog_off_val_lbl, s_dialog_off_val_buf);
    lv_obj_add_style(s_dialog_off_val_lbl, &ui_style_lbl_24_center, 0);
    lv_obj_set_pos(s_dialog_off_val_lbl, 208, 162);
    lv_obj_set_width(s_dialog_off_val_lbl, 132);

    lv_obj_t *btn_off_plus = lv_obj_create(s_dialog_subview_auto);
    lv_obj_remove_style_all(btn_off_plus);
    lv_obj_set_pos(btn_off_plus, 352, 152);
    lv_obj_set_size(btn_off_plus, 48, 48);
    lv_obj_add_style(btn_off_plus, &ui_style_soft_btn, 0);
    lv_obj_add_flag(btn_off_plus, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(btn_off_plus, on_thresh_step_click, LV_EVENT_SHORT_CLICKED, (void *)(uintptr_t)2);
    lv_obj_add_event_cb(btn_off_plus, on_thresh_step_click, LV_EVENT_LONG_PRESSED_REPEAT, (void *)(uintptr_t)2);
    lv_obj_t *lbl_off_plus = add_static_label(btn_off_plus, "+", &ui_font_24, UI_COLOR_INK);
    lv_obj_center(lbl_off_plus);

    /* Row 3 (y=212): Reset limits x=0,w=132,h=44; Inline validation x=144,y=212,w=256,h<=56 */
    lv_obj_t *btn_reset = lv_obj_create(s_dialog_subview_auto);
    lv_obj_remove_style_all(btn_reset);
    lv_obj_set_pos(btn_reset, 0, 212);
    lv_obj_set_size(btn_reset, 132, 44);
    lv_obj_add_style(btn_reset, &ui_style_soft_btn, 0);
    lv_obj_add_flag(btn_reset, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(btn_reset, on_dialog_reset_limits_click, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_reset = add_static_label(btn_reset, "Reset limits", &ui_font_18, UI_COLOR_INK);
    lv_obj_center(lbl_reset);

    s_dialog_validation_lbl = add_static_label(s_dialog_subview_auto, s_dialog_validation_buf, &ui_font_18, UI_COLOR_RED);
    lv_obj_set_pos(s_dialog_validation_lbl, 144, 212);
    lv_obj_set_size(s_dialog_validation_lbl, 256, 56);
    lv_label_set_long_mode(s_dialog_validation_lbl, LV_LABEL_LONG_WRAP);

    update_dialog_auto_subview();
    if (s_dialog_btn_x != NULL) lv_obj_move_foreground(s_dialog_btn_x);
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

    /* Modal overlay */
    s_dialog_overlay = lv_obj_create(s_screen);
    lv_obj_remove_style_all(s_dialog_overlay);
    lv_obj_set_size(s_dialog_overlay, 800, 480);
    lv_obj_set_style_bg_color(s_dialog_overlay, lv_color_hex(0x262D32), 0);
    lv_obj_set_style_bg_opa(s_dialog_overlay, 77, 0);

    /* Dialog Box (440x368, centered) */
    lv_obj_t *box = lv_obj_create(s_dialog_overlay);
    lv_obj_remove_style_all(box);
    lv_obj_set_size(box, 440, 368);
    lv_obj_center(box);
    lv_obj_add_style(box, &ui_style_card_chart, 0);
    lv_obj_set_style_pad_all(box, 20, 0);
    lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);
    s_dialog_box = box;

    /* Close (X) button on top right of box */
    lv_obj_t *btn_x = lv_obj_create(box);
    lv_obj_remove_style_all(btn_x);
    lv_obj_set_pos(btn_x, 400 - 44, 0);
    lv_obj_set_size(btn_x, 44, 44);
    lv_obj_add_style(btn_x, &ui_style_soft_btn, 0);
    lv_obj_add_flag(btn_x, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(btn_x, on_dialog_close_click, LV_EVENT_CLICKED, NULL);
    lv_obj_t *ic_x = add_icon(btn_x, &ui_icon_close_20, UI_COLOR_MUTED);
    lv_obj_center(ic_x);
    s_dialog_btn_x = btn_x;

    /* Shared Save settings button at (0, 280, 400, 48) on box */
    lv_obj_t *btn_save = lv_btn_create(box);
    lv_obj_remove_style_all(btn_save);
    lv_obj_set_pos(btn_save, 0, 280);
    lv_obj_set_size(btn_save, 400, 48);
    lv_obj_add_style(btn_save, &ui_style_action, 0);
    lv_obj_add_style(btn_save, &ui_style_action_disabled, LV_STATE_DISABLED);
    lv_obj_add_event_cb(btn_save, on_dialog_save_click, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_save = add_static_label(btn_save, "Save settings", &ui_font_24, lv_color_white());
    lv_obj_center(lbl_save);
    s_dialog_btn_save_dev = btn_save;
    s_dialog_btn_save_auto = btn_save;

    /* Build initial subview 1: Device configuration */
    build_dialog_device_subview(box);
}

static void close_device_dialog(void)
{
    cleanup_dialog_dd_scrim();
    if (s_dialog_preset_dd != NULL && lv_dropdown_is_open(s_dialog_preset_dd)) {
        lv_dropdown_close(s_dialog_preset_dd);
    }
    if (s_dialog_overlay != NULL) {
        lv_obj_del(s_dialog_overlay);
        s_dialog_overlay = NULL;
    }
    s_dialog_btn_x = NULL;
    s_dialog_box = NULL;
    s_dialog_device_idx = -1;
    s_dialog_subview_device = NULL;
    s_dialog_subview_auto = NULL;
    s_dialog_preset_dd = NULL;
    s_dialog_mode_btns[0] = NULL;
    s_dialog_mode_btns[1] = NULL;
    s_dialog_mode_labels[0] = NULL;
    s_dialog_mode_labels[1] = NULL;
    s_dialog_active_low_sw = NULL;
    s_dialog_btn_limits = NULL;
    s_dialog_mode_helper_lbl = NULL;
    s_dialog_auto_rule_lbl = NULL;
    s_dialog_on_op_lbl = NULL;
    s_dialog_off_op_lbl = NULL;
    s_dialog_on_val_lbl = NULL;
    s_dialog_off_val_lbl = NULL;
    s_dialog_validation_lbl = NULL;
    s_dialog_btn_save_auto = NULL;
    s_dialog_btn_save_dev = NULL;
}


/* Reset all per-page cached widget pointers to prevent stale references (P2.10) */
static void reset_page_widget_pointers(void)
{
    s_home_co2_val = NULL;
    s_home_co2_ppm_lbl = NULL;
    s_home_co2_badge = NULL;
    s_home_co2_badge_dot = NULL;
    s_home_co2_badge_lbl = NULL;
    s_home_co2_note = NULL;

    s_home_voc_val = NULL;
    s_home_voc_badge = NULL;
    s_home_voc_badge_dot = NULL;
    s_home_voc_badge_lbl = NULL;
    s_home_voc_note = NULL;

    s_home_temp_val = NULL;
    s_home_temp_note = NULL;
    s_home_humid_val = NULL;
    s_home_humid_note = NULL;

    for (uint8_t i = 0; i < UI_DEVICE_COUNT; i++) {
        s_home_dev_icons[i] = NULL;
        s_home_dev_names[i] = NULL;
        s_home_dev_states[i] = NULL;
        s_home_dev_modes[i] = NULL;
        s_home_dev_dashes[i] = NULL;
        s_home_dev_switches[i] = NULL;
        s_home_dev_retries[i] = NULL;

        s_dev_page_icons[i] = NULL;
        s_dev_page_names[i] = NULL;
        s_dev_page_modes[i] = NULL;
        s_dev_page_states[i] = NULL;
        s_dev_page_dashes[i] = NULL;
        s_dev_page_switches[i] = NULL;
        s_dev_page_retries[i] = NULL;
    }

    s_trends_cur_lbl = NULL;
    s_trends_min_lbl = NULL;
    s_trends_max_lbl = NULL;
    s_trends_unavail_badge = NULL;
    s_trends_chart = NULL;
    s_trends_chart_series = NULL;
    for (int i = 0; i < 3; i++) {
        s_trends_y_lbls[i] = NULL;
    }
    s_trends_tooltip = NULL;
    s_trends_empty_box = NULL;
    for (int i = 0; i < 4; i++) {
        s_trends_tab_btns[i] = NULL;
        s_trends_time_labels[i] = NULL;
    }
}

/* =========================================================================
 * Page Switcher & Container Lifecycle (P1.3 Async Navigation)
 * ========================================================================= */
static void do_navigate_to_page(ui_page_id_t page)
{
    if (s_dialog_overlay != NULL) close_device_dialog();

    if (page == s_current_page && s_page_container != NULL) return;

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
    lv_obj_set_size(s_page_container, 760, 336);
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
    s_selected_metric = metric;
    if (s_current_page == UI_PAGE_TRENDS && s_page_container != NULL) {
        update_trends_tabs();
        if (s_trends_tooltip != NULL) lv_obj_add_flag(s_trends_tooltip, LV_OBJ_FLAG_HIDDEN);
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
    return commit_device_config_internal(idx, cfg, is_boot, NULL, 0);
}

void ui_set_config_changed_cb(ui_config_changed_cb_t cb)
{
    s_config_changed_cb = cb;
}

void ui_feed_history(ui_metric_id_t metric, const int32_t *values, const bool *valid, uint16_t count, uint16_t start_minute)
{
    if (metric >= UI_METRIC_COUNT) return;

    /* Keep newest window (P1.8) */
    uint16_t offset = (count > UI_HISTORY_CAPACITY) ? (count - UI_HISTORY_CAPACITY) : 0u;
    uint16_t n = count - offset;

    /* Check if refeeding identical history */
    bool changed = false;
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
    /* P0.1: Must be 0xE9ECF1 (not 0xEFEFEF). ui_img_splash_logo has a baked background
     * without alpha that quantises in RGB565 to exact (239, 239, 247). Setting 0xE9ECF1
     * ensures identical (239, 239, 247) quantisation across the screen, eliminating the
     * visible bounding seam. This is a deliberate exception to the neutral canvas palette. */
    lv_obj_set_style_bg_color(s_splash_screen, lv_color_hex(0xE9ECF1), 0);
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
    }

    /* Outbound transmit gating: false until first fresh report is reconciled */
    s_tx_ready = false;

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

    /* Content Area (y=56..416, usable 760x336 with 20px H, 12px V padding) */
    s_content_area = lv_obj_create(s_screen);
    lv_obj_remove_style_all(s_content_area);
    lv_obj_set_pos(s_content_area, 20, 56 + 12);
    lv_obj_set_size(s_content_area, 760, 336);
    lv_obj_clear_flag(s_content_area, LV_OBJ_FLAG_SCROLLABLE);

    /* Permanent Bottom Navigation */
    build_navigation();

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
        update_uptime_string();
    }

    if (s_dialog_dd_scrim != NULL) {
        if (s_dialog_preset_dd == NULL || !lv_dropdown_is_open(s_dialog_preset_dd)) {
            cleanup_dialog_dd_scrim();
        }
    }

    bool online = is_link_connected(now);
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
        ui_announce("No response from node. Output state unknown.");
    }

    bool rx_changed = (lora_rx_revision != s_last_rendered_rx_revision);

    /* Evaluate Auto rules strictly on fresh received frames when online and tx_ready */
    if (rx_changed && online && s_tx_ready) {
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
    }

    /* Cheap change test: only refresh widgets when link state changed, commands changed, or new frame received */
    if (rx_changed || link_state_changed || cmd_state_changed) {
        s_last_rendered_rx_revision = lora_rx_revision;
        s_last_rendered_link_state = online;

        update_header();
        if (s_current_page == UI_PAGE_HOME) {
            update_home_page_widgets();
        } else if (s_current_page == UI_PAGE_TRENDS) {
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

        .stat_cur_buf = s_stat_cur_buf,
        .stat_min_buf = s_stat_min_buf,
        .stat_max_buf = s_stat_max_buf,
        .chart_tooltip_buf = s_chart_tooltip_buf,
        .co2_val_buf = s_co2_val_buf,
        .voc_val_buf = s_voc_val_buf,
        .temp_val_buf = s_temp_val_buf,
        .humid_val_buf = s_humid_val_buf,
        .co2_badge_buf = s_co2_badge_buf,
        .voc_badge_buf = s_voc_badge_buf,
        .dev_name_buf = s_dev_name_buf,
        .dev_state_buf = s_dev_state_buf,
        .dev_mode_buf = s_dev_mode_buf,

        .trends_chart = &s_trends_chart,
        .trends_chart_series = &s_trends_chart_series,
        .trends_tooltip = &s_trends_tooltip,
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
        .step_threshold = step_threshold,
        .on_dialog_reset_limits_click = on_dialog_reset_limits_click,
        .on_dialog_limits_back_click = on_dialog_limits_back_click,
        .on_dialog_open_limits_click = on_dialog_open_limits_click,

        .link_label = &s_link_label,
        .link_bars_cont = &s_link_bars_cont,
        .header_sep = &s_header_sep,
        .uptime_label = &s_uptime_label,
    };
    return &hooks;
}
#endif



