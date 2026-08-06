#ifndef IMAGES_H
#define IMAGES_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "lvgl.h"

// Pre-rendered static assets (see tools/build_assets.bat to regenerate).
// NOTE: the gauge dial-face image was removed — the finalized Dashboard gauge
// is a clean ring drawn with lv_arc (track + value) + an lv_line needle, no
// baked tick marks or 0..500 scale numbers, so no image is needed. The only
// pre-rendered asset left is the splash logo.
//
// Hyphen Deux brand logo for the splash screen: 380x126 INDEXED_8BIT,
// flattened onto the splash background color (tools/prep_logo.py).
LV_IMG_DECLARE(ui_img_logo);

// BLDC motor cross-section watermark (120x120 ALPHA_4BIT, tools/gen_motor_wm.py):
// the "E2" motif drawn faint + recolored behind the Dashboard speed number.
LV_IMG_DECLARE(ui_img_motor);

// Tab-bar icons: 24x24 ALPHA_4BIT masks matching the mockup's line-icon SVGs
// (tools/gen_icons.py). Alpha-only so one asset recolors per state (idle gray
// / active accent) via img_recolor, just like a font glyph.
LV_IMG_DECLARE(ui_icon_dash);
LV_IMG_DECLARE(ui_icon_mon);
LV_IMG_DECLARE(ui_icon_ctrl);
LV_IMG_DECLARE(ui_icon_graph);
LV_IMG_DECLARE(ui_icon_diag);
LV_IMG_DECLARE(ui_icon_set);

// Row/status icons (Monitor + Diagnostics), same 24x24 ALPHA_4BIT + recolor.
LV_IMG_DECLARE(ui_icon_bolt);
LV_IMG_DECLARE(ui_icon_battery);
LV_IMG_DECLARE(ui_icon_power);
LV_IMG_DECLARE(ui_icon_eff);
LV_IMG_DECLARE(ui_icon_thermo);
LV_IMG_DECLARE(ui_icon_crosshairs);
LV_IMG_DECLARE(ui_icon_encoder);
LV_IMG_DECLARE(ui_icon_link);
LV_IMG_DECLARE(ui_icon_check);
LV_IMG_DECLARE(ui_icon_xmark);
LV_IMG_DECLARE(ui_icon_chevron); // dropdown arrow (Settings)
LV_IMG_DECLARE(ui_icon_cw);      // clockwise rotation (direction button)
LV_IMG_DECLARE(ui_icon_ccw);     // counter-clockwise rotation (direction button)

#ifdef __cplusplus
}
#endif

#endif /* IMAGES_H */
