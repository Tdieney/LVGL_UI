#include "ui_theme.h"
#include "ui_fonts.h"

lv_style_t ui_style_canvas;
lv_style_t ui_style_card_co2;
lv_style_t ui_style_card_voc;
lv_style_t ui_style_card_temp;
lv_style_t ui_style_card_humid;
lv_style_t ui_style_card_device;
lv_style_t ui_style_card_chart;
lv_style_t ui_style_action;
lv_style_t ui_style_tab_inactive;
lv_style_t ui_style_tab_active;
lv_style_t ui_style_soft_btn;
lv_style_t ui_style_dd_list;
lv_style_t ui_style_dd_list_selected;
lv_style_t ui_style_dialog_btn_on;
lv_style_t ui_style_dialog_btn_off;
lv_style_t ui_style_action_disabled;
lv_style_t ui_style_lbl_18_ink;
lv_style_t ui_style_lbl_24_ink;
lv_style_t ui_style_lbl_18_muted;
lv_style_t ui_style_lbl_18_red;
lv_style_t ui_style_lbl_24_white;
lv_style_t ui_style_lbl_24_center;

static void setup_card_shadow(lv_style_t *style)
{
    /* Faint soft shadow: 0 2px 10px 0 rgba(24, 43, 77, 0.06) */
    lv_style_set_shadow_color(style, lv_color_hex(0x182B4D));
    lv_style_set_shadow_opa(style, 15);
    lv_style_set_shadow_width(style, 10);
    lv_style_set_shadow_ofs_x(style, 0);
    lv_style_set_shadow_ofs_y(style, 2);
    lv_style_set_shadow_spread(style, 0);
}

void ui_theme_init(void)
{
    /* Canvas flat background: #EFEFEF */
    lv_style_init(&ui_style_canvas);
    lv_style_set_bg_color(&ui_style_canvas, lv_color_hex(0xEFEFEF));
    lv_style_set_bg_opa(&ui_style_canvas, LV_OPA_COVER);
    lv_style_set_border_width(&ui_style_canvas, 0);
    lv_style_set_radius(&ui_style_canvas, 0);

    /* CO2 Card: flat #D6E7FF */
    lv_style_init(&ui_style_card_co2);
    lv_style_set_bg_color(&ui_style_card_co2, lv_color_hex(0xD6E7FF));
    lv_style_set_bg_opa(&ui_style_card_co2, LV_OPA_COVER);
    lv_style_set_border_width(&ui_style_card_co2, 0);
    lv_style_set_radius(&ui_style_card_co2, 20);
    setup_card_shadow(&ui_style_card_co2);

    /* VOC Card: flat #E7DBFF */
    lv_style_init(&ui_style_card_voc);
    lv_style_set_bg_color(&ui_style_card_voc, lv_color_hex(0xE7DBFF));
    lv_style_set_bg_opa(&ui_style_card_voc, LV_OPA_COVER);
    lv_style_set_border_width(&ui_style_card_voc, 0);
    lv_style_set_radius(&ui_style_card_voc, 20);
    setup_card_shadow(&ui_style_card_voc);

    /* Temperature Card: flat #FFEBCE */
    lv_style_init(&ui_style_card_temp);
    lv_style_set_bg_color(&ui_style_card_temp, lv_color_hex(0xFFEBCE));
    lv_style_set_bg_opa(&ui_style_card_temp, LV_OPA_COVER);
    lv_style_set_border_width(&ui_style_card_temp, 0);
    lv_style_set_radius(&ui_style_card_temp, 20);
    setup_card_shadow(&ui_style_card_temp);

    /* Humidity Card: flat #CEEFE7 */
    lv_style_init(&ui_style_card_humid);
    lv_style_set_bg_color(&ui_style_card_humid, lv_color_hex(0xCEEFE7));
    lv_style_set_bg_opa(&ui_style_card_humid, LV_OPA_COVER);
    lv_style_set_border_width(&ui_style_card_humid, 0);
    lv_style_set_radius(&ui_style_card_humid, 20);
    setup_card_shadow(&ui_style_card_humid);

    /* Device Card: flat #F7F7F7 */
    lv_style_init(&ui_style_card_device);
    lv_style_set_bg_color(&ui_style_card_device, lv_color_hex(0xF7F7F7));
    lv_style_set_bg_opa(&ui_style_card_device, LV_OPA_COVER);
    lv_style_set_border_width(&ui_style_card_device, 0);
    lv_style_set_radius(&ui_style_card_device, 20);
    setup_card_shadow(&ui_style_card_device);

    /* Chart & Settings Dialog Card: flat #FFFFFF */
    lv_style_init(&ui_style_card_chart);
    lv_style_set_bg_color(&ui_style_card_chart, lv_color_hex(0xFFFFFF));
    lv_style_set_bg_opa(&ui_style_card_chart, LV_OPA_COVER);
    lv_style_set_border_width(&ui_style_card_chart, 0);
    lv_style_set_radius(&ui_style_card_chart, 20);
    setup_card_shadow(&ui_style_card_chart);

    /* Primary Action / Selected Nav Fill: flat #215DDE */
    lv_style_init(&ui_style_action);
    lv_style_set_bg_color(&ui_style_action, lv_color_hex(0x215DDE));
    lv_style_set_bg_opa(&ui_style_action, LV_OPA_COVER);
    lv_style_set_border_width(&ui_style_action, 0);
    lv_style_set_radius(&ui_style_action, 14);

    /* Inactive metric tab: solid #DEEBF7, radius 12, border 0 */
    lv_style_init(&ui_style_tab_inactive);
    lv_style_set_bg_color(&ui_style_tab_inactive, lv_color_hex(0xDEEBF7));
    lv_style_set_bg_opa(&ui_style_tab_inactive, LV_OPA_COVER);
    lv_style_set_border_width(&ui_style_tab_inactive, 0);
    lv_style_set_radius(&ui_style_tab_inactive, 12);
    lv_style_set_text_color(&ui_style_tab_inactive, UI_COLOR_INK);

    /* Active metric tab: flat #215DDE, radius 12, border 0 */
    lv_style_init(&ui_style_tab_active);
    lv_style_set_bg_color(&ui_style_tab_active, lv_color_hex(0x215DDE));
    lv_style_set_bg_opa(&ui_style_tab_active, LV_OPA_COVER);
    lv_style_set_border_width(&ui_style_tab_active, 0);
    lv_style_set_radius(&ui_style_tab_active, 12);
    lv_style_set_text_color(&ui_style_tab_active, lv_color_white());

    /* Soft button (modal X, limits, steppers, reset): radius 10, #EFF3F7 */
    lv_style_init(&ui_style_soft_btn);
    lv_style_set_bg_color(&ui_style_soft_btn, UI_COLOR_NEUTRAL_SOFT);
    lv_style_set_bg_opa(&ui_style_soft_btn, LV_OPA_COVER);
    lv_style_set_border_width(&ui_style_soft_btn, 0);
    lv_style_set_radius(&ui_style_soft_btn, 10);

    /* Open dropdown list: flat white, font 18, line space 19, shadow, radius 10, pad 12/8 */
    lv_style_init(&ui_style_dd_list);
    lv_style_set_text_font(&ui_style_dd_list, &ui_font_18);
    lv_style_set_text_line_space(&ui_style_dd_list, 19);
    lv_style_set_border_width(&ui_style_dd_list, 0);
    lv_style_set_radius(&ui_style_dd_list, 10);
    lv_style_set_bg_color(&ui_style_dd_list, lv_color_hex(0xFFFFFF));
    lv_style_set_bg_opa(&ui_style_dd_list, LV_OPA_COVER);
    lv_style_set_text_color(&ui_style_dd_list, UI_COLOR_INK);
    lv_style_set_shadow_width(&ui_style_dd_list, 16);
    lv_style_set_shadow_color(&ui_style_dd_list, lv_color_black());
    lv_style_set_shadow_opa(&ui_style_dd_list, 40);
    lv_style_set_shadow_ofs_y(&ui_style_dd_list, 4);
    lv_style_set_pad_hor(&ui_style_dd_list, 12);
    lv_style_set_pad_ver(&ui_style_dd_list, 8);
    lv_style_set_max_height(&ui_style_dd_list, 240);

    /* Dropdown list selected item: blue soft fill, blue text */
    lv_style_init(&ui_style_dd_list_selected);
    lv_style_set_text_font(&ui_style_dd_list_selected, &ui_font_18);
    lv_style_set_text_line_space(&ui_style_dd_list_selected, 19);
    lv_style_set_bg_color(&ui_style_dd_list_selected, UI_COLOR_BLUE_SOFT);
    lv_style_set_bg_opa(&ui_style_dd_list_selected, LV_OPA_COVER);
    lv_style_set_text_color(&ui_style_dd_list_selected, UI_COLOR_BLUE_TEXT);

    /* Dialog mode selected button: #EFF3FF, radius 10 */
    lv_style_init(&ui_style_dialog_btn_on);
    lv_style_set_bg_color(&ui_style_dialog_btn_on, UI_COLOR_BLUE_SOFT);
    lv_style_set_bg_opa(&ui_style_dialog_btn_on, LV_OPA_COVER);
    lv_style_set_border_width(&ui_style_dialog_btn_on, 0);
    lv_style_set_radius(&ui_style_dialog_btn_on, 10);

    /* Dialog mode unselected button: #F7F7F7, radius 10 */
    lv_style_init(&ui_style_dialog_btn_off);
    lv_style_set_bg_color(&ui_style_dialog_btn_off, lv_color_hex(0xF7F7F7));
    lv_style_set_bg_opa(&ui_style_dialog_btn_off, LV_OPA_COVER);
    lv_style_set_border_width(&ui_style_dialog_btn_off, 0);
    lv_style_set_radius(&ui_style_dialog_btn_off, 10);

    /* Action button disabled state */
    lv_style_init(&ui_style_action_disabled);
    lv_style_set_bg_color(&ui_style_action_disabled, UI_COLOR_SWITCH_OFF);
    lv_style_set_bg_opa(&ui_style_action_disabled, LV_OPA_COVER);

    /* Shared static label styles to eliminate per-label local style allocations */
    lv_style_init(&ui_style_lbl_18_ink);
    lv_style_set_text_font(&ui_style_lbl_18_ink, &ui_font_18);
    lv_style_set_text_color(&ui_style_lbl_18_ink, UI_COLOR_INK);

    lv_style_init(&ui_style_lbl_24_ink);
    lv_style_set_text_font(&ui_style_lbl_24_ink, &ui_font_24);
    lv_style_set_text_color(&ui_style_lbl_24_ink, UI_COLOR_INK);

    lv_style_init(&ui_style_lbl_18_muted);
    lv_style_set_text_font(&ui_style_lbl_18_muted, &ui_font_18);
    lv_style_set_text_color(&ui_style_lbl_18_muted, UI_COLOR_MUTED);

    lv_style_init(&ui_style_lbl_18_red);
    lv_style_set_text_font(&ui_style_lbl_18_red, &ui_font_18);
    lv_style_set_text_color(&ui_style_lbl_18_red, UI_COLOR_RED);

    lv_style_init(&ui_style_lbl_24_white);
    lv_style_set_text_font(&ui_style_lbl_24_white, &ui_font_24);
    lv_style_set_text_color(&ui_style_lbl_24_white, lv_color_white());

    lv_style_init(&ui_style_lbl_24_center);
    lv_style_set_text_font(&ui_style_lbl_24_center, &ui_font_24);
    lv_style_set_text_color(&ui_style_lbl_24_center, UI_COLOR_INK);
    lv_style_set_text_align(&ui_style_lbl_24_center, LV_TEXT_ALIGN_CENTER);
}
