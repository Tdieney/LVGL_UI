/**
 * lv_conf.h for the PC simulator.
 *
 * Deliberately mirrors the target MCU constraints (80MHz, 128KB RAM) so
 * memory problems show up on the PC before they show up on hardware.
 * Anything not defined here falls back to LVGL defaults via lv_conf_internal.h.
 */
#ifndef LV_CONF_H
#define LV_CONF_H

#include "../ui_mcu_profile.h"

#define LV_COLOR_DEPTH 16
#define LV_COLOR_16_SWAP 0

/* Validated compact profile for the real firmware (see PERF.md). Static label
 * text lives in Flash and numeric labels use fixed buffers, so 52 KB retains
 * >10 KB headroom on the worst (Control) screen. */
#define LV_MEM_CUSTOM 0
#define LV_MEM_SIZE UI_LVGL_HEAP_BYTES

#define LV_DISP_DEF_REFR_PERIOD 30
#define LV_INDEV_DEF_READ_PERIOD 30

#define LV_TICK_CUSTOM 0 /* lv_tick_inc() driven manually from main.c */
#define LV_DPI_DEF 130

#define LV_USE_LOG 1
#define LV_LOG_LEVEL LV_LOG_LEVEL_WARN
#define LV_LOG_PRINTF 1

/* FPS/CPU overlay, enabled with -DSIM_PERF_MONITOR (kept out of screenshots) */
#ifdef SIM_PERF_MONITOR
#define LV_USE_PERF_MONITOR 1
#endif

// Only size 20 is used (LV_SYMBOL_* glyphs on buttons/dropdowns, see the
// custom-font gotcha in CLAUDE.md) — every custom UI text also sits at a
// 20px floor now, so this stays the single built-in size needed. 12/14/16
// dropped to avoid compiling in unused font tables.
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_DEFAULT &lv_font_montserrat_20

#endif /* LV_CONF_H */
