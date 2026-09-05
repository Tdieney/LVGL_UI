#include "ui.h"

static lv_obj_t *s_screen;

static lv_obj_t *add_static_label(lv_obj_t *parent, const char *text,
                                  const lv_font_t *font, lv_color_t color)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text_static(label, text);
    lv_obj_set_style_text_font(label, font, LV_PART_MAIN);
    lv_obj_set_style_text_color(label, color, LV_PART_MAIN);
    return label;
}

void ui_init(void)
{
    lv_disp_t *display = lv_disp_get_default();
    lv_theme_t *theme = lv_theme_default_init(
        display, lv_palette_main(LV_PALETTE_BLUE),
        lv_palette_main(LV_PALETTE_BLUE_GREY), false, LV_FONT_DEFAULT);
    lv_disp_set_theme(display, theme);

    s_screen = lv_obj_create(NULL);
    lv_obj_clear_flag(s_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(s_screen, lv_color_hex(0xF4F6F8), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_screen, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_screen, 0, LV_PART_MAIN);

    lv_obj_t *header = lv_obj_create(s_screen);
    lv_obj_remove_style_all(header);
    lv_obj_set_size(header, 800, 64);
    lv_obj_set_style_bg_color(header, lv_color_hex(0x17324F), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(header, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_t *title = add_static_label(header, "SMART HUB", LV_FONT_DEFAULT,
                                       lv_color_white());
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 24, 0);

    lv_obj_t *card = lv_obj_create(s_screen);
    lv_obj_set_size(card, 520, 220);
    lv_obj_center(card);
    lv_obj_set_style_radius(card, 14, LV_PART_MAIN);
    lv_obj_set_style_border_width(card, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(card, lv_color_hex(0xD6DCE3), LV_PART_MAIN);
    lv_obj_set_style_bg_color(card, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(card, 18, LV_PART_MAIN);

    add_static_label(card, "UI definition pending", LV_FONT_DEFAULT,
                     lv_color_hex(0x17324F));
    add_static_label(card, "Hardware baseline: LCD | LoRa | Smart Nodes",
                     LV_FONT_DEFAULT, lv_color_hex(0x5B6573));
    add_static_label(card, "Next: agree on functions and workflow",
                     LV_FONT_DEFAULT, lv_color_hex(0x5B6573));

    lv_scr_load(s_screen);
}

void ui_tick(void)
{
    /* Live data lanes will be added after the product model is agreed. */
}
