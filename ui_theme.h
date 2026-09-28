#ifndef UI_THEME_H
#define UI_THEME_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Quiet Architecture: calibrated RGB565 palette matching concept medians.
 * Token calibration table:
 * Role                 Source RGB888   RGB565  Decoded RGB888  Notes
 * UI_COLOR_CANVAS      #EEECE7         0xEF7C  #EFEBE7         Warm stone canvas (median #EEECE7)
 * UI_COLOR_SURFACE     #F3F1EC         0xF79D  #F7F3EE         Warm porcelain card surface (median #F3F1EC)
 * UI_COLOR_FOREST      #214539         0x2227  #214539         Upper hero & primary action (median #23443B)
 * UI_COLOR_FOREST_DARK #183831         0x19C6  #183831         Lower hero gradient candidate (median #19372F)
 * UI_COLOR_CELADON     #D7E0BC         0xD717  #D6E3B8         Active nav pill & celadon accent (median #D7E0BC)
 * UI_COLOR_INK         #17251F         0x1124  #102421         Near-black ink for text & icons
 */
#define UI_COLOR_CANVAS         lv_color_hex(0xEEECE7)  /* Warm stone canvas (measured median #EEECE7) */
#define UI_COLOR_SURFACE        lv_color_hex(0xF3F1EC)  /* Warm porcelain card surfaces (measured median #F3F1EC) */
#define UI_COLOR_FOREST         lv_color_hex(0x214539)  /* Dark forest upper hero & primary buttons */
#define UI_COLOR_FOREST_DARK    lv_color_hex(0x183831)  /* Dark forest lower hero */
#define UI_COLOR_CELADON        lv_color_hex(0xD7E0BC)  /* Warm sage/olive celadon accents & active nav pill */
#define UI_COLOR_CELADON_STRIP  lv_color_hex(0xDFE7CB)  /* Warm celadon sensor strip */
#define UI_COLOR_INK            lv_color_hex(0x17251F)  /* Dark ink text, titles & icons */
#define UI_COLOR_MUTED          lv_color_hex(0x59665A)  /* Muted warm secondary labels & units */
#define UI_COLOR_DIVIDER        lv_color_hex(0xE5E0D5)  /* Fine warm row & section dividers */
#define UI_COLOR_OUTLINE        lv_color_hex(0xD8D1C2)  /* Restrained warm card & button outlines */
#define UI_COLOR_ERROR          lv_color_hex(0xC93B2B)  /* Error outlines & warning messages */
#define UI_COLOR_ERROR_SOFT     lv_color_hex(0xFDE8E8)  /* Soft error tint */
#define UI_COLOR_GREEN          lv_color_hex(0x1E7E34)  /* Checkmark circle & switch ON */
#define UI_COLOR_AMBER          lv_color_hex(0x8A5000)  /* Amber status */
#define UI_COLOR_SWITCH_OFF     lv_color_hex(0x939E94)  /* Switch OFF track */

/* Compatibility aliases for existing UI references */
#define UI_COLOR_BLUE           UI_COLOR_FOREST
#define UI_COLOR_BLUE_TEXT      UI_COLOR_FOREST
#define UI_COLOR_BLUE_SOFT      UI_COLOR_CELADON
#define UI_COLOR_GREEN_SOFT     UI_COLOR_CELADON_STRIP
#define UI_COLOR_AMBER_SOFT     lv_color_hex(0xFFF3DE)
#define UI_COLOR_RED            UI_COLOR_ERROR
#define UI_COLOR_RED_SOFT       UI_COLOR_ERROR_SOFT
#define UI_COLOR_NEUTRAL_SOFT   UI_COLOR_DIVIDER
#define UI_COLOR_LINE           UI_COLOR_DIVIDER
#define UI_COLOR_ICON_BLUE      UI_COLOR_FOREST
#define UI_COLOR_PLATE_BLUE     UI_COLOR_CELADON

/* Sensor & Metric Colors */
#define UI_COLOR_CO2_BG         UI_COLOR_FOREST
#define UI_COLOR_VOC_BG         UI_COLOR_FOREST
#define UI_COLOR_TEMP_BG        UI_COLOR_SURFACE
#define UI_COLOR_HUMID_BG       UI_COLOR_SURFACE
#define UI_COLOR_METRIC_TAB_BG  UI_COLOR_SURFACE
#define UI_COLOR_CO2_FG         lv_color_white()
#define UI_COLOR_VOC_FG         lv_color_white()
#define UI_COLOR_TEMP_ICON      UI_COLOR_FOREST
#define UI_COLOR_HUMID_ICON     UI_COLOR_FOREST

/* Shared immutable styles placed in Flash (LVGL 8.4 const-style tables) */
extern const lv_style_t ui_style_pressed;
extern const lv_style_t ui_style_nav_btn_pressed;
extern const lv_style_t ui_style_canvas;
extern const lv_style_t ui_style_card_hero;
extern const lv_style_t ui_style_card_porcelain;
extern const lv_style_t ui_style_strip_celadon;
extern const lv_style_t ui_style_btn_forest;
extern const lv_style_t ui_style_btn_outlined;
extern const lv_style_t ui_style_field_outlined;
extern const lv_style_t ui_style_field_error;
extern const lv_style_t ui_style_stepper_btn;
extern const lv_style_t ui_style_dd_list;
extern const lv_style_t ui_style_dd_list_selected;
extern const lv_style_t ui_style_action_disabled;
extern const lv_style_t ui_style_lbl_18_ink;
extern const lv_style_t ui_style_lbl_24_ink;
extern const lv_style_t ui_style_lbl_18_muted;
extern const lv_style_t ui_style_lbl_18_red;
extern const lv_style_t ui_style_lbl_24_white;
extern const lv_style_t ui_style_lbl_18_blue;
extern const lv_style_t ui_style_lbl_18_blue_text;
extern const lv_style_t ui_style_lbl_24_amber;
extern const lv_style_t ui_style_lbl_24_muted;
extern const lv_style_t ui_style_lbl_34_ink;
extern const lv_style_t ui_style_lbl_48_white;
extern const lv_style_t ui_style_lbl_48_ink;
extern const lv_style_t ui_style_icon_blue;
extern const lv_style_t ui_style_icon_muted;
extern const lv_style_t ui_style_icon_white;
extern const lv_style_t ui_style_dialog_overlay;
extern const lv_style_t ui_style_dialog_dropdown;
extern const lv_style_t ui_style_switch_main;
extern const lv_style_t ui_style_switch_indicator;
extern const lv_style_t ui_style_dd_label;

/* Pinned LVGL 8.4 lv_obj_add_style and lv_obj_remove_style take non-const lv_style_t *
 * even though they do not mutate style descriptors. Transparent casts allow const lv_style_t
 * to be used without compiler warnings or mutability violations. */
#define lv_obj_add_style(obj, style, selector) \
    (lv_obj_add_style)((obj), (lv_style_t *)(uintptr_t)(style), (selector))
#define lv_obj_remove_style(obj, style, selector) \
    (lv_obj_remove_style)((obj), (lv_style_t *)(uintptr_t)(style), (selector))

/* Initialize all theme styles once at startup */
void ui_theme_init(void);

#ifdef __cplusplus
}
#endif

#endif /* UI_THEME_H */
