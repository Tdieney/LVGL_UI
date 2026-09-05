#ifndef UI_MCU_PROFILE_H
#define UI_MCU_PROFILE_H

/* Starting profile inherited from the proven 800x480 RGB565 xSPI UI.
 * Revalidate heap and draw-buffer choices against the final Smart Hub firmware. */
#define UI_DISPLAY_HOR_RES  800u
#define UI_DISPLAY_VER_RES  480u
#define UI_COLOR_BYTES      2u
#define UI_DRAW_BUF_LINES   10u
#define UI_DRAW_BUF_PIXELS  (UI_DISPLAY_HOR_RES * UI_DRAW_BUF_LINES)
#define UI_DRAW_BUF_BYTES   (UI_DRAW_BUF_PIXELS * UI_COLOR_BYTES)
#define UI_LVGL_HEAP_BYTES  (42u * 1024u)

#endif /* UI_MCU_PROFILE_H */
