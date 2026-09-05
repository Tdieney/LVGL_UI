#ifndef UI_FONTS_H
#define UI_FONTS_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "lvgl.h"

LV_FONT_DECLARE(ui_font_mono38)  // JetBrains Mono Bold 38px - Dashboard Run Time digits/colon

// Custom fonts generated with lv_font_conv (see tools/build_assets.bat).
// Type scale (2026 legibility pass): hero 36->44, a single merged 20px Bold
// label tier (replaces the old 12/14 split), and a 22px Bold value tier
// (replaces the old 20px Regular) — no custom UI text below 20px anymore.
LV_FONT_DECLARE(ui_font_mono66)  // JetBrains Mono Bold 66px — digits ".,-% " only (Dashboard RPM + Control SPEED/TORQUE heroes, ~50% bigger)
LV_FONT_DECLARE(ui_font_mono56)  // JetBrains Mono Bold 56px — digits ".,-% " only (Dashboard gauge RPM hero)
LV_FONT_DECLARE(ui_font_mono44)  // JetBrains Mono Bold 44px — digits ".,-% " only (gauge RPM)
LV_FONT_DECLARE(ui_font_mono30)  // JetBrains Mono Bold 30px — digits ".,-% " only (Dashboard card values)
LV_FONT_DECLARE(ui_font_mono22)  // JetBrains Mono Bold 22px — full ASCII (readout values)
LV_FONT_DECLARE(ui_font_mono20)  // JetBrains Mono Bold 20px — full ASCII (labels, badges — floor size)

// Caption/button sans: Segoe UI Semibold. Proportional captions, section
// headers, buttons and tab labels — numeric readouts keep the mono fonts above
// so "label vs value" reads at a glance. See tools/build_assets.bat; swap the
// TTF for an OFL font before a customer release if licensing matters.
LV_FONT_DECLARE(ui_font_sans22)  // Segoe UI Semibold 22px — full ASCII + degree (primary buttons)
LV_FONT_DECLARE(ui_font_sans20)  // Segoe UI Semibold 20px — full ASCII + degree (captions, card titles)

// NOTE: the FontAwesome icon font (ui_font_icons20) was removed — every icon
// in the app (tab bar + Monitor/Diagnostics rows + status marks) is now a
// custom ALPHA_4BIT image drawn by tools/gen_icons.py to match the mockup's
// SVG shapes, recolored per state via img_recolor. See images.h (ui_icon_*).
// Built-in LVGL Montserrat glyphs (LV_SYMBOL_SAVE / _REFRESH on the Settings
// buttons) are unrelated and still used from &lv_font_montserrat_20.

#ifdef __cplusplus
}
#endif

#endif /* UI_FONTS_H */
