#ifndef LV_CONF_H
#define LV_CONF_H

#include "../ui_mcu_profile.h"

#define LV_COLOR_DEPTH 16
#define LV_COLOR_16_SWAP 0
#define LV_MEM_CUSTOM 0
#define LV_MEM_SIZE UI_LVGL_HEAP_BYTES

/* Study 12: all surfaces are flat fills; no gradients exist in UI. Gradient cache disabled. */
#define LV_GRAD_CACHE_DEF_SIZE 0

#define LV_DISP_DEF_REFR_PERIOD 30
#define LV_INDEV_DEF_READ_PERIOD 30
#define LV_TICK_CUSTOM 0
#define LV_DPI_DEF 130
#define LV_USE_LOG 1
#define LV_LOG_LEVEL LV_LOG_LEVEL_WARN
#define LV_LOG_PRINTF 1

/* Production fonts: Segoe UI Semibold (18, 24, 34, 62) are self-contained in ui_fonts.c.
 * Montserrat 16..34 are disabled to conserve ~132.5 KB flash. */
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 0
#define LV_FONT_MONTSERRAT_18 0
#define LV_FONT_MONTSERRAT_20 0
#define LV_FONT_MONTSERRAT_24 0
#define LV_FONT_MONTSERRAT_30 0
#define LV_FONT_MONTSERRAT_34 0
#define LV_FONT_MONTSERRAT_48 0
#define LV_FONT_DEFAULT &lv_font_montserrat_14

#endif /* LV_CONF_H */
