#include "ui_theme.h"
#include "ui_fonts.h"

/* RGB888 to RGB565 at compile time, honouring the display's byte order. */
#if LV_COLOR_16_SWAP == 0
#define UI_HEX(c) { .full = (uint16_t)(((uint32_t)(c) & 0xF80000u) >> 8 | ((uint32_t)(c) & 0xFC00u) >> 5 | ((uint32_t)(c) & 0xFFu) >> 3) }
#else
#define UI_HEX(c) { .full = (uint16_t)(((uint32_t)(c) & 0xF80000u) >> 16 | ((uint32_t)(c) & 0xFC00u) >> 13 | ((uint32_t)(c) & 0x1C00u) << 3 | ((uint32_t)(c) & 0xF8u) << 5) }
#endif

#define NUM(p, n)   { .prop = (p), .value = { .num = (n) } }
#define COLOR(p, c) { .prop = (p), .value = { .color = UI_HEX(c) } }
#define FONT(f)     { .prop = LV_STYLE_TEXT_FONT, .value = { .ptr = &(f) } }
#define STYLE(name, ...) \
    static const lv_style_const_prop_t name##_props[] = { __VA_ARGS__ }; \
    LV_STYLE_CONST_INIT(name, name##_props)

/* ---- Base & containers --------------------------------------------------- */
STYLE(ui_style_base,
      NUM(LV_STYLE_PAD_TOP, 0), NUM(LV_STYLE_PAD_BOTTOM, 0),
      NUM(LV_STYLE_PAD_LEFT, 0), NUM(LV_STYLE_PAD_RIGHT, 0),
      NUM(LV_STYLE_BORDER_WIDTH, 0), NUM(LV_STYLE_BG_OPA, LV_OPA_TRANSP),
      NUM(LV_STYLE_SHADOW_WIDTH, 0), NUM(LV_STYLE_OUTLINE_WIDTH, 0),
      NUM(LV_STYLE_RADIUS, 0), NUM(LV_STYLE_ANIM_TIME, 0), FONT(ui_font_48));

STYLE(ui_style_canvas,
      COLOR(LV_STYLE_BG_COLOR, UI_RGB_CANVAS), NUM(LV_STYLE_BG_OPA, LV_OPA_COVER),
      COLOR(LV_STYLE_TEXT_COLOR, UI_RGB_INK));

STYLE(ui_style_tile,
      COLOR(LV_STYLE_BG_COLOR, UI_RGB_SURFACE), NUM(LV_STYLE_BG_OPA, LV_OPA_COVER),
      NUM(LV_STYLE_BORDER_WIDTH, 1), COLOR(LV_STYLE_BORDER_COLOR, UI_RGB_OUTLINE),
      NUM(LV_STYLE_RADIUS, 24));

STYLE(ui_style_dot_idle,
      COLOR(LV_STYLE_BG_COLOR, UI_RGB_OUTLINE), NUM(LV_STYLE_BG_OPA, LV_OPA_COVER),
      NUM(LV_STYLE_RADIUS, LV_RADIUS_CIRCLE));

STYLE(ui_style_dot_cyan,
      COLOR(LV_STYLE_BG_COLOR, UI_RGB_CYAN), NUM(LV_STYLE_BG_OPA, LV_OPA_COVER),
      NUM(LV_STYLE_RADIUS, LV_RADIUS_CIRCLE));

/* ---- Buttons ------------------------------------------------------------- */
STYLE(ui_style_btn_header,
      COLOR(LV_STYLE_BG_COLOR, UI_RGB_SURFACE), NUM(LV_STYLE_BG_OPA, LV_OPA_COVER),
      NUM(LV_STYLE_BORDER_WIDTH, 1), COLOR(LV_STYLE_BORDER_COLOR, UI_RGB_OUTLINE),
      NUM(LV_STYLE_RADIUS, 12));

STYLE(ui_style_btn_pressed,
      COLOR(LV_STYLE_BG_COLOR, UI_RGB_PRESSED));

STYLE(ui_style_btn_disabled,
      COLOR(LV_STYLE_BG_COLOR, UI_RGB_DISABLED), COLOR(LV_STYLE_BORDER_COLOR, UI_RGB_OUTLINE),
      NUM(LV_STYLE_BORDER_WIDTH, 1));

STYLE(ui_style_ac_toggle_on,
      COLOR(LV_STYLE_BG_COLOR, UI_RGB_CYAN_SOFT),
      NUM(LV_STYLE_BORDER_WIDTH, 2), COLOR(LV_STYLE_BORDER_COLOR, UI_RGB_CYAN));

STYLE(ui_style_ac_stepper,
      COLOR(LV_STYLE_BG_COLOR, UI_RGB_SURFACE), NUM(LV_STYLE_BG_OPA, LV_OPA_COVER),
      NUM(LV_STYLE_BORDER_WIDTH, 1), COLOR(LV_STYLE_BORDER_COLOR, UI_RGB_OUTLINE),
      NUM(LV_STYLE_RADIUS, 20));

STYLE(ui_style_circle_btn,
      COLOR(LV_STYLE_BG_COLOR, UI_RGB_SURFACE), NUM(LV_STYLE_BG_OPA, LV_OPA_COVER),
      NUM(LV_STYLE_BORDER_WIDTH, 1), COLOR(LV_STYLE_BORDER_COLOR, UI_RGB_OUTLINE),
      NUM(LV_STYLE_RADIUS, LV_RADIUS_CIRCLE));

STYLE(ui_style_sun_ring_on,
      COLOR(LV_STYLE_BG_COLOR, UI_RGB_GOLD_SOFT),
      NUM(LV_STYLE_BORDER_WIDTH, 2), COLOR(LV_STYLE_BORDER_COLOR, UI_RGB_GOLD));

STYLE(ui_style_sun_core,
      NUM(LV_STYLE_BG_OPA, LV_OPA_TRANSP),
      NUM(LV_STYLE_BORDER_WIDTH, 2), COLOR(LV_STYLE_BORDER_COLOR, UI_RGB_MUTED),
      NUM(LV_STYLE_RADIUS, LV_RADIUS_CIRCLE));

STYLE(ui_style_sun_core_on,
      COLOR(LV_STYLE_BG_COLOR, UI_RGB_GOLD), NUM(LV_STYLE_BG_OPA, LV_OPA_COVER),
      NUM(LV_STYLE_BORDER_WIDTH, 0));

STYLE(ui_style_scene_selected,
      COLOR(LV_STYLE_BG_COLOR, UI_RGB_GOLD), COLOR(LV_STYLE_BORDER_COLOR, UI_RGB_GOLD));

STYLE(ui_style_scene_selected_pressed,
      COLOR(LV_STYLE_BG_COLOR, UI_RGB_GOLD_PRESSED), COLOR(LV_STYLE_BORDER_COLOR, UI_RGB_GOLD_PRESSED));

STYLE(ui_style_pill_cyan,
      COLOR(LV_STYLE_BG_COLOR, UI_RGB_CYAN), NUM(LV_STYLE_BG_OPA, LV_OPA_COVER),
      NUM(LV_STYLE_BORDER_WIDTH, 0), NUM(LV_STYLE_RADIUS, 26));

STYLE(ui_style_pill_cyan_pressed,
      COLOR(LV_STYLE_BG_COLOR, UI_RGB_CYAN_PRESSED));

STYLE(ui_style_pill_pending,
      COLOR(LV_STYLE_BG_COLOR, UI_RGB_GOLD_SOFT),
      NUM(LV_STYLE_BORDER_WIDTH, 1), COLOR(LV_STYLE_BORDER_COLOR, UI_RGB_GOLD));

STYLE(ui_style_pill_failed,
      COLOR(LV_STYLE_BG_COLOR, UI_RGB_ERROR_SOFT),
      NUM(LV_STYLE_BORDER_WIDTH, 1), COLOR(LV_STYLE_BORDER_COLOR, UI_RGB_ERROR));

/* ---- Icon colors --------------------------------------------------------- */
#define ICON_STYLE(name, rgb) \
    STYLE(name, COLOR(LV_STYLE_IMG_RECOLOR, rgb), NUM(LV_STYLE_IMG_RECOLOR_OPA, LV_OPA_COVER))
ICON_STYLE(ui_style_icon_ink, UI_RGB_INK);
ICON_STYLE(ui_style_icon_muted, UI_RGB_MUTED);
ICON_STYLE(ui_style_icon_idle, UI_RGB_ICON_IDLE);
ICON_STYLE(ui_style_icon_dark, UI_RGB_DARK);
ICON_STYLE(ui_style_icon_ray_off, UI_RGB_RAY_OFF);
ICON_STYLE(ui_style_icon_gold, UI_RGB_GOLD);
ICON_STYLE(ui_style_icon_gold_half, UI_RGB_GOLD_HALF);
ICON_STYLE(ui_style_icon_cyan, UI_RGB_CYAN);
ICON_STYLE(ui_style_icon_coral, UI_RGB_CORAL);
ICON_STYLE(ui_style_icon_error, UI_RGB_ERROR);

/* ---- Text ---------------------------------------------------------------- */
STYLE(ui_style_text_48, FONT(ui_font_48));
STYLE(ui_style_text_76, FONT(ui_font_digits_76));
STYLE(ui_style_text_48_env, FONT(ui_font_48), NUM(LV_STYLE_PAD_TOP, 4));
STYLE(ui_style_color_ink, COLOR(LV_STYLE_TEXT_COLOR, UI_RGB_INK));
STYLE(ui_style_color_muted, COLOR(LV_STYLE_TEXT_COLOR, UI_RGB_MUTED));
STYLE(ui_style_text_center, NUM(LV_STYLE_TEXT_ALIGN, LV_TEXT_ALIGN_CENTER));

void ui_theme_init(void)
{
    /* Flat RGB565 fills only: no gradient cache. */
    lv_gradient_set_cache_size(0);
}
