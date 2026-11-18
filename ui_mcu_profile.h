#ifndef UI_MCU_PROFILE_H
#define UI_MCU_PROFILE_H

/* ESP32-S3 with the course 3.5-inch ST7796 SPI LCD (480 x 320, RGB565) and
 * FT6336 touch. One partial draw buffer; never a full framebuffer on the MCU.
 * Revalidate heap and draw-buffer choices against the complete firmware. */
#define UI_DISPLAY_HOR_RES  480u
#define UI_DISPLAY_VER_RES  320u
#define UI_COLOR_BYTES      2u
#ifndef UI_DRAW_BUF_LINES
#define UI_DRAW_BUF_LINES   20u
#endif
#define UI_DRAW_BUF_PIXELS  (UI_DISPLAY_HOR_RES * UI_DRAW_BUF_LINES)
#define UI_DRAW_BUF_BYTES   (UI_DRAW_BUF_PIXELS * UI_COLOR_BYTES)
#ifndef UI_LVGL_HEAP_BYTES
#define UI_LVGL_HEAP_BYTES  (48u * 1024u)
#endif

/* Physical active area of the panel, used by the simulator's real-size view:
 * 73.44 x 48.96 mm, 0.153 mm pixel pitch, about 166 pixels per inch. */
#define UI_PANEL_WIDTH_UM   73440u
#define UI_PANEL_HEIGHT_UM  48960u

#endif /* UI_MCU_PROFILE_H */
