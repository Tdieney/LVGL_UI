#include "ui_theme.h"
#include "ui_fonts.h"

/* Helper macro to encode RGB888 hex as RGB565 compile-time constant.
 * Matches lv_color_hex() across both LV_COLOR_16_SWAP=0 and LV_COLOR_16_SWAP=1. */
#if LV_COLOR_16_SWAP == 0
#define UI_HEX(c) { .full = (uint16_t)(((uint32_t)(c) & 0xF80000u) >> 8 | ((uint32_t)(c) & 0xFC00u) >> 5 | ((uint32_t)(c) & 0xFFu) >> 3) }
#else
#define UI_HEX(c) { .full = (uint16_t)(((uint32_t)(c) & 0xF80000u) >> 16 | ((uint32_t)(c) & 0xFC00u) >> 13 | ((uint32_t)(c) & 0x1C00u) << 3 | ((uint32_t)(c) & 0xF8u) << 5) }
#endif
#define UI_WHITE  { .full = 0xFFFF }
#define UI_BLACK  { .full = 0x0000 }

/* 1. Pressed states */
static const lv_style_const_prop_t s_pressed_props[] = {
    { .prop = LV_STYLE_BG_OPA, .value = { .num = LV_OPA_70 } },
};
LV_STYLE_CONST_INIT(ui_style_pressed, s_pressed_props);

static const lv_style_const_prop_t s_nav_btn_pressed_props[] = {
    { .prop = LV_STYLE_BG_COLOR, .value = { .color = UI_HEX(0xD7E0BC) } },
    { .prop = LV_STYLE_BG_OPA, .value = { .num = LV_OPA_40 } },
};
LV_STYLE_CONST_INIT(ui_style_nav_btn_pressed, s_nav_btn_pressed_props);

/* 2. Canvas */
static const lv_style_const_prop_t s_canvas_props[] = {
    { .prop = LV_STYLE_BG_COLOR, .value = { .color = UI_HEX(0xEEECE7) } },
    { .prop = LV_STYLE_BG_OPA, .value = { .num = LV_OPA_COVER } },
    { .prop = LV_STYLE_BORDER_WIDTH, .value = { .num = 0 } },
    { .prop = LV_STYLE_RADIUS, .value = { .num = 0 } },
};
LV_STYLE_CONST_INIT(ui_style_canvas, s_canvas_props);

/* 3. Hero Card (Forest fill) */
static const lv_style_const_prop_t s_card_hero_props[] = {
    { .prop = LV_STYLE_BG_COLOR, .value = { .color = UI_HEX(0x214539) } },
    { .prop = LV_STYLE_BG_OPA, .value = { .num = LV_OPA_COVER } },
    { .prop = LV_STYLE_BORDER_WIDTH, .value = { .num = 0 } },
    { .prop = LV_STYLE_RADIUS, .value = { .num = 16 } },
};
LV_STYLE_CONST_INIT(ui_style_card_hero, s_card_hero_props);

/* 4. Porcelain Card */
static const lv_style_const_prop_t s_card_porcelain_props[] = {
    { .prop = LV_STYLE_BG_COLOR, .value = { .color = UI_HEX(0xF3F1EC) } },
    { .prop = LV_STYLE_BG_OPA, .value = { .num = LV_OPA_COVER } },
    { .prop = LV_STYLE_BORDER_WIDTH, .value = { .num = 1 } },
    { .prop = LV_STYLE_BORDER_COLOR, .value = { .color = UI_HEX(0xD8D1C2) } },
    { .prop = LV_STYLE_RADIUS, .value = { .num = 16 } },
};
LV_STYLE_CONST_INIT(ui_style_card_porcelain, s_card_porcelain_props);

/* 5. Celadon Sensor Strip */
static const lv_style_const_prop_t s_strip_celadon_props[] = {
    { .prop = LV_STYLE_BG_COLOR, .value = { .color = UI_HEX(0xDFE7CB) } },
    { .prop = LV_STYLE_BG_OPA, .value = { .num = LV_OPA_COVER } },
    { .prop = LV_STYLE_BORDER_WIDTH, .value = { .num = 1 } },
    { .prop = LV_STYLE_BORDER_COLOR, .value = { .color = UI_HEX(0xD7E0BC) } },
    { .prop = LV_STYLE_RADIUS, .value = { .num = 12 } },
};
LV_STYLE_CONST_INIT(ui_style_strip_celadon, s_strip_celadon_props);

/* 6. Buttons */
static const lv_style_const_prop_t s_btn_forest_props[] = {
    { .prop = LV_STYLE_BG_COLOR, .value = { .color = UI_HEX(0x214539) } },
    { .prop = LV_STYLE_BG_OPA, .value = { .num = LV_OPA_COVER } },
    { .prop = LV_STYLE_BORDER_WIDTH, .value = { .num = 0 } },
    { .prop = LV_STYLE_RADIUS, .value = { .num = 12 } },
    { .prop = LV_STYLE_TEXT_COLOR, .value = { .color = UI_WHITE } },
    { .prop = LV_STYLE_TEXT_FONT, .value = { .ptr = &ui_font_24 } },
};
LV_STYLE_CONST_INIT(ui_style_btn_forest, s_btn_forest_props);

static const lv_style_const_prop_t s_btn_outlined_props[] = {
    { .prop = LV_STYLE_BG_COLOR, .value = { .color = UI_HEX(0xF3F1EC) } },
    { .prop = LV_STYLE_BG_OPA, .value = { .num = LV_OPA_COVER } },
    { .prop = LV_STYLE_BORDER_WIDTH, .value = { .num = 1 } },
    { .prop = LV_STYLE_BORDER_COLOR, .value = { .color = UI_HEX(0xD8D1C2) } },
    { .prop = LV_STYLE_RADIUS, .value = { .num = 12 } },
    { .prop = LV_STYLE_TEXT_COLOR, .value = { .color = UI_HEX(0x17251F) } },
    { .prop = LV_STYLE_TEXT_FONT, .value = { .ptr = &ui_font_24 } },
};
LV_STYLE_CONST_INIT(ui_style_btn_outlined, s_btn_outlined_props);

static const lv_style_const_prop_t s_stepper_btn_props[] = {
    { .prop = LV_STYLE_BG_COLOR, .value = { .color = UI_HEX(0xF3F1EC) } },
    { .prop = LV_STYLE_BG_OPA, .value = { .num = LV_OPA_COVER } },
    { .prop = LV_STYLE_BORDER_WIDTH, .value = { .num = 1 } },
    { .prop = LV_STYLE_BORDER_COLOR, .value = { .color = UI_HEX(0xD8D1C2) } },
    { .prop = LV_STYLE_RADIUS, .value = { .num = 12 } },
    { .prop = LV_STYLE_TEXT_COLOR, .value = { .color = UI_HEX(0x17251F) } },
    { .prop = LV_STYLE_TEXT_FONT, .value = { .ptr = &ui_font_24 } },
};
LV_STYLE_CONST_INIT(ui_style_stepper_btn, s_stepper_btn_props);

/* 7. Fields */
static const lv_style_const_prop_t s_field_outlined_props[] = {
    { .prop = LV_STYLE_BG_COLOR, .value = { .color = UI_HEX(0xF3F1EC) } },
    { .prop = LV_STYLE_BG_OPA, .value = { .num = LV_OPA_COVER } },
    { .prop = LV_STYLE_BORDER_WIDTH, .value = { .num = 1 } },
    { .prop = LV_STYLE_BORDER_COLOR, .value = { .color = UI_HEX(0xD8D1C2) } },
    { .prop = LV_STYLE_RADIUS, .value = { .num = 10 } },
};
LV_STYLE_CONST_INIT(ui_style_field_outlined, s_field_outlined_props);

static const lv_style_const_prop_t s_field_error_props[] = {
    { .prop = LV_STYLE_BG_COLOR, .value = { .color = UI_HEX(0xF3F1EC) } },
    { .prop = LV_STYLE_BG_OPA, .value = { .num = LV_OPA_COVER } },
    { .prop = LV_STYLE_BORDER_WIDTH, .value = { .num = 2 } },
    { .prop = LV_STYLE_BORDER_COLOR, .value = { .color = UI_HEX(0xC93B2B) } },
    { .prop = LV_STYLE_RADIUS, .value = { .num = 10 } },
};
LV_STYLE_CONST_INIT(ui_style_field_error, s_field_error_props);

/* 8. Dropdown List */
static const lv_style_const_prop_t s_dd_list_props[] = {
    { .prop = LV_STYLE_TEXT_FONT, .value = { .ptr = &ui_font_18 } },
    { .prop = LV_STYLE_TEXT_LINE_SPACE, .value = { .num = 21 } },
    { .prop = LV_STYLE_BORDER_WIDTH, .value = { .num = 0 } },
    { .prop = LV_STYLE_RADIUS, .value = { .num = 10 } },
    { .prop = LV_STYLE_BG_COLOR, .value = { .color = UI_HEX(0xF3F1EC) } },
    { .prop = LV_STYLE_BG_OPA, .value = { .num = LV_OPA_COVER } },
    { .prop = LV_STYLE_TEXT_COLOR, .value = { .color = UI_HEX(0x17251F) } },
    { .prop = LV_STYLE_SHADOW_WIDTH, .value = { .num = 16 } },
    { .prop = LV_STYLE_SHADOW_COLOR, .value = { .color = UI_BLACK } },
    { .prop = LV_STYLE_SHADOW_OPA, .value = { .num = 40 } },
    { .prop = LV_STYLE_SHADOW_OFS_Y, .value = { .num = 4 } },
    { .prop = LV_STYLE_PAD_LEFT, .value = { .num = 12 } },
    { .prop = LV_STYLE_PAD_RIGHT, .value = { .num = 12 } },
    { .prop = LV_STYLE_PAD_TOP, .value = { .num = 8 } },
    { .prop = LV_STYLE_PAD_BOTTOM, .value = { .num = 8 } },
    { .prop = LV_STYLE_MAX_HEIGHT, .value = { .num = 240 } },
};
LV_STYLE_CONST_INIT(ui_style_dd_list, s_dd_list_props);

static const lv_style_const_prop_t s_dd_list_selected_props[] = {
    { .prop = LV_STYLE_TEXT_FONT, .value = { .ptr = &ui_font_18 } },
    { .prop = LV_STYLE_TEXT_LINE_SPACE, .value = { .num = 21 } },
    { .prop = LV_STYLE_BG_COLOR, .value = { .color = UI_HEX(0xD7E0BC) } },
    { .prop = LV_STYLE_BG_OPA, .value = { .num = LV_OPA_COVER } },
    { .prop = LV_STYLE_TEXT_COLOR, .value = { .color = UI_HEX(0x214539) } },
};
LV_STYLE_CONST_INIT(ui_style_dd_list_selected, s_dd_list_selected_props);

static const lv_style_const_prop_t s_action_disabled_props[] = {
    { .prop = LV_STYLE_BG_COLOR, .value = { .color = UI_HEX(0x939E94) } },
    { .prop = LV_STYLE_BG_OPA, .value = { .num = LV_OPA_COVER } },
};
LV_STYLE_CONST_INIT(ui_style_action_disabled, s_action_disabled_props);

/* 9. Static Labels */
static const lv_style_const_prop_t s_lbl_18_ink_props[] = {
    { .prop = LV_STYLE_TEXT_FONT, .value = { .ptr = &ui_font_18 } },
    { .prop = LV_STYLE_TEXT_COLOR, .value = { .color = UI_HEX(0x17251F) } },
};
LV_STYLE_CONST_INIT(ui_style_lbl_18_ink, s_lbl_18_ink_props);

static const lv_style_const_prop_t s_lbl_24_ink_props[] = {
    { .prop = LV_STYLE_TEXT_FONT, .value = { .ptr = &ui_font_24 } },
    { .prop = LV_STYLE_TEXT_COLOR, .value = { .color = UI_HEX(0x17251F) } },
};
LV_STYLE_CONST_INIT(ui_style_lbl_24_ink, s_lbl_24_ink_props);

static const lv_style_const_prop_t s_lbl_18_muted_props[] = {
    { .prop = LV_STYLE_TEXT_FONT, .value = { .ptr = &ui_font_18 } },
    { .prop = LV_STYLE_TEXT_COLOR, .value = { .color = UI_HEX(0x59665A) } },
};
LV_STYLE_CONST_INIT(ui_style_lbl_18_muted, s_lbl_18_muted_props);

static const lv_style_const_prop_t s_lbl_18_red_props[] = {
    { .prop = LV_STYLE_TEXT_FONT, .value = { .ptr = &ui_font_18 } },
    { .prop = LV_STYLE_TEXT_COLOR, .value = { .color = UI_HEX(0xC93B2B) } },
};
LV_STYLE_CONST_INIT(ui_style_lbl_18_red, s_lbl_18_red_props);

static const lv_style_const_prop_t s_lbl_24_white_props[] = {
    { .prop = LV_STYLE_TEXT_FONT, .value = { .ptr = &ui_font_24 } },
    { .prop = LV_STYLE_TEXT_COLOR, .value = { .color = UI_WHITE } },
};
LV_STYLE_CONST_INIT(ui_style_lbl_24_white, s_lbl_24_white_props);

static const lv_style_const_prop_t s_lbl_18_blue_props[] = {
    { .prop = LV_STYLE_TEXT_FONT, .value = { .ptr = &ui_font_18 } },
    { .prop = LV_STYLE_TEXT_COLOR, .value = { .color = UI_HEX(0x214539) } },
};
LV_STYLE_CONST_INIT(ui_style_lbl_18_blue, s_lbl_18_blue_props);

static const lv_style_const_prop_t s_lbl_18_blue_text_props[] = {
    { .prop = LV_STYLE_TEXT_FONT, .value = { .ptr = &ui_font_18 } },
    { .prop = LV_STYLE_TEXT_COLOR, .value = { .color = UI_HEX(0x214539) } },
};
LV_STYLE_CONST_INIT(ui_style_lbl_18_blue_text, s_lbl_18_blue_text_props);

static const lv_style_const_prop_t s_lbl_24_amber_props[] = {
    { .prop = LV_STYLE_TEXT_FONT, .value = { .ptr = &ui_font_24 } },
    { .prop = LV_STYLE_TEXT_COLOR, .value = { .color = UI_HEX(0x8A5000) } },
};
LV_STYLE_CONST_INIT(ui_style_lbl_24_amber, s_lbl_24_amber_props);

static const lv_style_const_prop_t s_lbl_24_muted_props[] = {
    { .prop = LV_STYLE_TEXT_FONT, .value = { .ptr = &ui_font_24 } },
    { .prop = LV_STYLE_TEXT_COLOR, .value = { .color = UI_HEX(0x59665A) } },
};
LV_STYLE_CONST_INIT(ui_style_lbl_24_muted, s_lbl_24_muted_props);

static const lv_style_const_prop_t s_lbl_34_ink_props[] = {
    { .prop = LV_STYLE_TEXT_FONT, .value = { .ptr = &ui_font_34 } },
    { .prop = LV_STYLE_TEXT_COLOR, .value = { .color = UI_HEX(0x17251F) } },
};
LV_STYLE_CONST_INIT(ui_style_lbl_34_ink, s_lbl_34_ink_props);

static const lv_style_const_prop_t s_lbl_48_white_props[] = {
    { .prop = LV_STYLE_TEXT_FONT, .value = { .ptr = &ui_font_48 } },
    { .prop = LV_STYLE_TEXT_COLOR, .value = { .color = UI_WHITE } },
};
LV_STYLE_CONST_INIT(ui_style_lbl_48_white, s_lbl_48_white_props);

static const lv_style_const_prop_t s_lbl_48_ink_props[] = {
    { .prop = LV_STYLE_TEXT_FONT, .value = { .ptr = &ui_font_48 } },
    { .prop = LV_STYLE_TEXT_COLOR, .value = { .color = UI_HEX(0x17251F) } },
};
LV_STYLE_CONST_INIT(ui_style_lbl_48_ink, s_lbl_48_ink_props);

/* 10. Icons */
static const lv_style_const_prop_t s_icon_blue_props[] = {
    { .prop = LV_STYLE_IMG_RECOLOR, .value = { .color = UI_HEX(0x214539) } },
    { .prop = LV_STYLE_IMG_RECOLOR_OPA, .value = { .num = LV_OPA_COVER } },
};
LV_STYLE_CONST_INIT(ui_style_icon_blue, s_icon_blue_props);

static const lv_style_const_prop_t s_icon_muted_props[] = {
    { .prop = LV_STYLE_IMG_RECOLOR, .value = { .color = UI_HEX(0x59665A) } },
    { .prop = LV_STYLE_IMG_RECOLOR_OPA, .value = { .num = LV_OPA_COVER } },
};
LV_STYLE_CONST_INIT(ui_style_icon_muted, s_icon_muted_props);

static const lv_style_const_prop_t s_icon_white_props[] = {
    { .prop = LV_STYLE_IMG_RECOLOR, .value = { .color = UI_WHITE } },
    { .prop = LV_STYLE_IMG_RECOLOR_OPA, .value = { .num = LV_OPA_COVER } },
};
LV_STYLE_CONST_INIT(ui_style_icon_white, s_icon_white_props);

/* 11. Modal Overlay */
static const lv_style_const_prop_t s_dialog_overlay_props[] = {
    { .prop = LV_STYLE_BG_COLOR, .value = { .color = UI_HEX(0xEEECE7) } },
    { .prop = LV_STYLE_BG_OPA, .value = { .num = LV_OPA_COVER } },
};
LV_STYLE_CONST_INIT(ui_style_dialog_overlay, s_dialog_overlay_props);

/* 12. Modal Dropdown Button */
static const lv_style_const_prop_t s_dialog_dropdown_props[] = {
    { .prop = LV_STYLE_BORDER_WIDTH, .value = { .num = 1 } },
    { .prop = LV_STYLE_BORDER_COLOR, .value = { .color = UI_HEX(0xD8D1C2) } },
    { .prop = LV_STYLE_RADIUS, .value = { .num = 10 } },
    { .prop = LV_STYLE_BG_COLOR, .value = { .color = UI_HEX(0xF3F1EC) } },
    { .prop = LV_STYLE_BG_OPA, .value = { .num = LV_OPA_COVER } },
    { .prop = LV_STYLE_TEXT_FONT, .value = { .ptr = &ui_font_18 } },
    { .prop = LV_STYLE_PAD_LEFT, .value = { .num = 12 } },
    { .prop = LV_STYLE_PAD_RIGHT, .value = { .num = 12 } },
    { .prop = LV_STYLE_PAD_TOP, .value = { .num = 10 } },
    { .prop = LV_STYLE_PAD_BOTTOM, .value = { .num = 10 } },
};
LV_STYLE_CONST_INIT(ui_style_dialog_dropdown, s_dialog_dropdown_props);

/* 13. Switch */
static const lv_style_const_prop_t s_switch_main_props[] = {
    { .prop = LV_STYLE_BG_COLOR, .value = { .color = UI_HEX(0x939E94) } },
    { .prop = LV_STYLE_ANIM_TIME, .value = { .num = 0 } },
};
LV_STYLE_CONST_INIT(ui_style_switch_main, s_switch_main_props);

static const lv_style_const_prop_t s_switch_indicator_props[] = {
    { .prop = LV_STYLE_BG_COLOR, .value = { .color = UI_HEX(0x214539) } },
};
LV_STYLE_CONST_INIT(ui_style_switch_indicator, s_switch_indicator_props);

/* 14. Dropdown list label */
static const lv_style_const_prop_t s_dd_label_props[] = {
    { .prop = LV_STYLE_TEXT_FONT, .value = { .ptr = &ui_font_18 } },
    { .prop = LV_STYLE_TEXT_LINE_SPACE, .value = { .num = 21 } },
};
LV_STYLE_CONST_INIT(ui_style_dd_label, s_dd_label_props);

void ui_theme_init(void)
{
    /* Gradient cache disabled for flat RGB565 fills */
    lv_gradient_set_cache_size(0);
}
