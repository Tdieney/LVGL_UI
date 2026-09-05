#ifndef IMAGES_H
#define IMAGES_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "lvgl.h"

// Pre-rendered static assets (see tools/build_assets.bat to regenerate).
// Hyphen Deux brand logo for the splash screen: 380x126 INDEXED_8BIT,
// flattened onto the splash background color. Canonical prepared source:
// tools/assets/ui_logo.png.
LV_IMG_DECLARE(ui_img_logo);

// POC placeholder for the pre-rendered header band (see screens.c
// UI_TOPBAR_IMG_POC). Full-rect ALPHA_4BIT mask, recolored to COLOR_CHROME at
// runtime. TODO: replace with real pre-rendered band art before shipping.
LV_IMG_DECLARE(ui_img_topbar);

// Small chip used as a topbar link/status graphic (POC placeholder).
LV_IMG_DECLARE(ui_img_topbar_chip);

// Dashboard gauge face: 320x320 ALPHA_4BIT (outer arc, labels, ticks, inner ring,
// faint motor motif). The live green arc,
// RPM number and direction icon remain lightweight LVGL widgets above it.
LV_IMG_DECLARE(ui_img_dashboard_gauge);

// Sidebar icons: 44x44 ALPHA_4BIT masks matching the mockup's line-icon SVGs
// (tools/gen_icons.py). Alpha-only so one asset recolors per state (idle gray
// / active accent) via img_recolor, just like a font glyph.
LV_IMG_DECLARE(ui_icon_dash);
LV_IMG_DECLARE(ui_icon_mon);
LV_IMG_DECLARE(ui_icon_ctrl);
LV_IMG_DECLARE(ui_icon_graph);
LV_IMG_DECLARE(ui_icon_diag);
LV_IMG_DECLARE(ui_icon_set);

// Row/status icons used by current screens, same 24x24 ALPHA_4BIT + recolor.
LV_IMG_DECLARE(ui_icon_bolt);
LV_IMG_DECLARE(ui_icon_thermo);
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
