#ifndef UI_MCU_PROFILE_H
#define UI_MCU_PROFILE_H

/* Validated memory profile for the 800x480 RGB565 / xSPI target.
 *
 * Use UI_LVGL_HEAP_BYTES as LV_MEM_SIZE in the firmware's lv_conf.h and use
 * UI_DRAW_BUF_PIXELS for ONE DMA-capable display draw buffer. The display
 * module has its own GRAM, so a full framebuffer on the MCU is unnecessary.
 */
#define UI_DISPLAY_HOR_RES  800u
#define UI_DISPLAY_VER_RES  480u
#define UI_COLOR_BYTES      2u
#define UI_DRAW_BUF_LINES   10u
#define UI_DRAW_BUF_PIXELS  (UI_DISPLAY_HOR_RES * UI_DRAW_BUF_LINES)
#define UI_DRAW_BUF_BYTES   (UI_DRAW_BUF_PIXELS * UI_COLOR_BYTES)
#define UI_LVGL_HEAP_BYTES  (42u * 1024u)

#endif /* UI_MCU_PROFILE_H */
