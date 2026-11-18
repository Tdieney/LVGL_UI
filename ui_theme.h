#ifndef UI_THEME_H
#define UI_THEME_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Zero-text dark UI palette. Flat RGB888 tokens rendered as RGB565;
 * every style is const-initialized in Flash (ui_theme.c).
 *
 * Token          RGB888    Role
 * CANVAS         #050608   Full-screen background
 * SURFACE        #101216   Corner buttons, +/- steppers, icon tiles, idle circles
 * PRESSED        #1A1E26   Pressed state on dark surface buttons
 * DISABLED       #0B0D10   Disabled button fill
 * OUTLINE        #242830   1 px crisp border on dark surface buttons and tiles
 * RAY_OFF        #1C2028   Unlit radial sun rays on Light page
 * INK            #F5F7FA   Primary hero numbers and active stepper/action icons
 * MUTED          #58606E   Off sun core/AC, unavailable reading, disabled icons
 * ICON_IDLE      #A5ACB8   Header corner icons and unselected scene icons
 * DARK           #050608   Dark icon color on solid GOLD or CYAN fills
 * GOLD           #FFCE54   Lit sun core/ring, full rays, selected scene, pending baud
 * GOLD_PRESSED   #E6B842   Pressed selected Gold scene circle
 * GOLD_HALF      #8C702E   Half-step leading sun ray color
 * GOLD_SOFT      #1F1A0C   Lit sun outer halo and pending baud pill background
 * CYAN           #48DBFB   Active AC, humidity drop, apply pill, RX/active dots
 * CYAN_PRESSED   #32C5E6   Pressed solid Cyan apply pill
 * CYAN_SOFT      #0E2C38   Active AC snowflake tile background
 * CORAL          #FF7E67   Valid room temperature thermometer icon
 * ERROR          #FF6B6B   Failed baud pill border and cross icon
 * ERROR_SOFT     #261114   Failed baud pill background
 */
#define UI_RGB_CANVAS        0x050608u
#define UI_RGB_SURFACE       0x101216u
#define UI_RGB_PRESSED       0x1A1E26u
#define UI_RGB_DISABLED      0x0B0D10u
#define UI_RGB_OUTLINE       0x242830u
#define UI_RGB_RAY_OFF       0x1C2028u
#define UI_RGB_INK           0xF5F7FAu
#define UI_RGB_MUTED         0x58606Eu
#define UI_RGB_ICON_IDLE     0xA5ACB8u
#define UI_RGB_DARK          0x050608u
#define UI_RGB_GOLD          0xFFCE54u
#define UI_RGB_GOLD_PRESSED  0xE6B842u
#define UI_RGB_GOLD_HALF     0x8C702Eu
#define UI_RGB_GOLD_SOFT     0x1F1A0Cu
#define UI_RGB_CYAN          0x48DBFBu
#define UI_RGB_CYAN_PRESSED  0x32C5E6u
#define UI_RGB_CYAN_SOFT     0x0E2C38u
#define UI_RGB_CORAL         0xFF7E67u
#define UI_RGB_ERROR         0xFF6B6Bu
#define UI_RGB_ERROR_SOFT    0x261114u

/* Base & containers */
extern const lv_style_t ui_style_base;
extern const lv_style_t ui_style_canvas;
extern const lv_style_t ui_style_tile;
extern const lv_style_t ui_style_dot_idle;
extern const lv_style_t ui_style_dot_cyan;

/* Buttons */
extern const lv_style_t ui_style_btn_header;
extern const lv_style_t ui_style_btn_pressed;
extern const lv_style_t ui_style_btn_disabled;
extern const lv_style_t ui_style_ac_toggle_on;
extern const lv_style_t ui_style_ac_stepper;
extern const lv_style_t ui_style_circle_btn;
extern const lv_style_t ui_style_sun_ring_on;
extern const lv_style_t ui_style_sun_core;
extern const lv_style_t ui_style_sun_core_on;
extern const lv_style_t ui_style_scene_selected;
extern const lv_style_t ui_style_scene_selected_pressed;
extern const lv_style_t ui_style_pill_cyan;
extern const lv_style_t ui_style_pill_cyan_pressed;
extern const lv_style_t ui_style_pill_pending;
extern const lv_style_t ui_style_pill_failed;

/* Icon colors (A4 images drawn with img_recolor) */
extern const lv_style_t ui_style_icon_ink;
extern const lv_style_t ui_style_icon_muted;
extern const lv_style_t ui_style_icon_idle;
extern const lv_style_t ui_style_icon_dark;
extern const lv_style_t ui_style_icon_ray_off;
extern const lv_style_t ui_style_icon_gold;
extern const lv_style_t ui_style_icon_gold_half;
extern const lv_style_t ui_style_icon_cyan;
extern const lv_style_t ui_style_icon_coral;
extern const lv_style_t ui_style_icon_error;

/* Text */
extern const lv_style_t ui_style_text_48;
extern const lv_style_t ui_style_text_76;
extern const lv_style_t ui_style_text_48_env;
extern const lv_style_t ui_style_color_ink;
extern const lv_style_t ui_style_color_muted;
extern const lv_style_t ui_style_text_center;

/* LVGL 8.4 lv_obj_add_style/lv_obj_remove_style take a non-const lv_style_t *
 * but never write through it. The casts let const Flash styles be added. */
#define lv_obj_add_style(obj, style, selector) \
    (lv_obj_add_style)((obj), (lv_style_t *)(uintptr_t)(style), (selector))
#define lv_obj_remove_style(obj, style, selector) \
    (lv_obj_remove_style)((obj), (lv_style_t *)(uintptr_t)(style), (selector))

void ui_theme_init(void);

#ifdef __cplusplus
}
#endif

#endif /* UI_THEME_H */
