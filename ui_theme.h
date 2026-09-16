#ifndef UI_THEME_H
#define UI_THEME_H

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Color Tokens (Study 12 RGB565-exact) */
#define UI_COLOR_INK            lv_color_hex(0x182B4D)
#define UI_COLOR_MUTED          lv_color_hex(0x42546B)
#define UI_COLOR_BLUE           lv_color_hex(0x1865E7)
#define UI_COLOR_BLUE_TEXT      lv_color_hex(0x1552B0)
#define UI_COLOR_BLUE_SOFT      lv_color_hex(0xEFF3FF)
#define UI_COLOR_GREEN          lv_color_hex(0x197047)
#define UI_COLOR_GREEN_SOFT     lv_color_hex(0xE7F7EF)
#define UI_COLOR_AMBER          lv_color_hex(0x8A5000)
#define UI_COLOR_AMBER_SOFT     lv_color_hex(0xFFF3DE)
#define UI_COLOR_RED            lv_color_hex(0xA12B25)
#define UI_COLOR_RED_SOFT       lv_color_hex(0xFFEFEF)
#define UI_COLOR_NEUTRAL_SOFT   lv_color_hex(0xEFF3F7)
#define UI_COLOR_LINE           lv_color_hex(0xDEE7F7)
#define UI_COLOR_ICON_BLUE      lv_color_hex(0x2470F0)
#define UI_COLOR_PLATE_BLUE     lv_color_hex(0xDEEBFF)
#define UI_COLOR_SWITCH_OFF     lv_color_hex(0xBDC7D6)

/* Sensor & Metric Foreground Colors */
#define UI_COLOR_CO2_FG         lv_color_hex(0x153E79)
#define UI_COLOR_VOC_FG         lv_color_hex(0x483277)
#define UI_COLOR_TEMP_ICON      lv_color_hex(0x985116)
#define UI_COLOR_HUMID_ICON     lv_color_hex(0x17665D)

/* Shared styles (Study 12 flat fills, no gradients) */
extern lv_style_t ui_style_canvas;
extern lv_style_t ui_style_card_co2;
extern lv_style_t ui_style_card_voc;
extern lv_style_t ui_style_card_temp;
extern lv_style_t ui_style_card_humid;
extern lv_style_t ui_style_card_device;
extern lv_style_t ui_style_card_chart;
extern lv_style_t ui_style_action;
extern lv_style_t ui_style_tab_inactive;
extern lv_style_t ui_style_tab_active;
extern lv_style_t ui_style_soft_btn;
extern lv_style_t ui_style_dd_list;
extern lv_style_t ui_style_dd_list_selected;
extern lv_style_t ui_style_dialog_btn_on;
extern lv_style_t ui_style_dialog_btn_off;
extern lv_style_t ui_style_action_disabled;
extern lv_style_t ui_style_lbl_18_ink;
extern lv_style_t ui_style_lbl_24_ink;
extern lv_style_t ui_style_lbl_18_muted;
extern lv_style_t ui_style_lbl_18_red;
extern lv_style_t ui_style_lbl_24_white;
extern lv_style_t ui_style_lbl_24_center;

/* Initialize all theme styles once at startup */
void ui_theme_init(void);

#ifdef __cplusplus
}
#endif

#endif /* UI_THEME_H */
