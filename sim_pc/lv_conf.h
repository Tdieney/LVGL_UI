#ifndef LV_CONF_H
#define LV_CONF_H

/* Simulator profile. The firmware keeps its own lv_conf.h and display port;
 * match these values there (16-bit color, heap, no themes, no gradients). */
#include "../ui_mcu_profile.h"

#define LV_COLOR_DEPTH 16
#define LV_COLOR_16_SWAP 0
#define LV_MEM_CUSTOM 0
#define LV_MEM_SIZE UI_LVGL_HEAP_BYTES

/* All surfaces are flat fills; no gradient cache. */
#define LV_GRAD_CACHE_DEF_SIZE 0

#define LV_DISP_DEF_REFR_PERIOD 20
#define LV_INDEV_DEF_READ_PERIOD 20
#define LV_TICK_CUSTOM 0
#define LV_DPI_DEF 166
#define LV_USE_LOG 1
#define LV_LOG_LEVEL LV_LOG_LEVEL_WARN
#define LV_LOG_PRINTF 1

/* Visible text uses the Plus Jakarta Sans tables in ui_fonts.c. Montserrat 14
 * stays enabled only as LVGL's internal default. */
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_DEFAULT &lv_font_montserrat_14

/* Every widget is styled by ui_theme.c. */
#define LV_USE_THEME_DEFAULT 0
#define LV_USE_THEME_BASIC 0
#define LV_USE_THEME_MONO 0

#endif /* LV_CONF_H */
